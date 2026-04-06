#include <iostream>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

#include "../Client/protocol.h"
#include "flightcalc.h"

using namespace std;

// Structured internal telemetry record
struct TelemetryRecord
{
    uint32_t planeId;
    uint64_t timestamp;
    float fuel;
};

// Active flight states, one per plane
static unordered_map<uint32_t, FlightState> g_activeFlights;

// Receive exactly 'size' bytes from the socket
int recvAll(SOCKET sock, char* buffer, int size)
{
    int total = 0;

    while (total < size)
    {
        int bytes = recv(sock, buffer + total, size - total, 0);
        if (bytes <= 0)
            return bytes;

        total += bytes;
    }

    return total;
}

// Validation helpers
static bool ValidatePlaneId(uint32_t planeId)
{
    return planeId != 0;
}

static bool ValidateTimestamp(uint64_t timestamp)
{
    return timestamp != 0;
}

static bool ValidateFuel(float fuel)
{
    return fuel >= 0.0f && fuel <= 100000.0f;
}

// Build structured internal record from incoming data
static bool BuildTelemetryRecord(uint32_t planeId, const TelemetryPayload& payload, TelemetryRecord& record)
{
    record.planeId = planeId;
    record.timestamp = payload.timestamp;
    record.fuel = payload.fuel;

    if (!ValidatePlaneId(record.planeId))
    {
        cout << "[Serialization] Invalid plane ID: " << record.planeId << "\n";
        return false;
    }

    if (!ValidateTimestamp(record.timestamp))
    {
        cout << "[Serialization] Invalid timestamp for plane "
            << record.planeId << ": " << record.timestamp << "\n";
        return false;
    }

    if (!ValidateFuel(record.fuel))
    {
        cout << "[Serialization] Invalid fuel value for plane "
            << record.planeId << ": " << record.fuel << "\n";
        return false;
    }

    return true;
}

bool HandleFlightStart(uint32_t planeId)
{
    if (!ValidatePlaneId(planeId))
    {
        cout << "[Serialization] Invalid plane ID on FLIGHT_START: " << planeId << "\n";
        return false;
    }

    FlightState state;
    InitializeFlightState(state, planeId);
    g_activeFlights[planeId] = state;

    cout << "[Serialization] Flight initialized for plane " << planeId << "\n";
    return true;
}

bool HandleTelemetryMessage(SOCKET clientSocket, uint32_t planeId)
{
    if (!ValidatePlaneId(planeId))
    {
        cout << "[Serialization] Invalid plane ID in telemetry header: " << planeId << "\n";
        return false;
    }

    // Read the raw telemetry payload fully
    char rawPayload[sizeof(TelemetryPayload)];
    int res = recvAll(clientSocket, rawPayload, sizeof(rawPayload));

    if (res <= 0)
    {
        cout << "[Serialization] Failed to read telemetry payload for plane " << planeId << "\n";
        return false;
    }

    if (res != sizeof(TelemetryPayload))
    {
        cout << "[Serialization] Incomplete telemetry payload for plane " << planeId << "\n";
        return false;
    }

    // Deserialize raw bytes into payload structure
    TelemetryPayload payload;
    memcpy(&payload, rawPayload, sizeof(TelemetryPayload));

    // Create structured internal telemetry record
    TelemetryRecord record;
    if (!BuildTelemetryRecord(planeId, payload, record))
    {
        cout << "[Serialization] Telemetry packet discarded for plane " << planeId << "\n";
        return false;
    }

    // If telemetry comes before FLIGHT_START, auto-create state
    auto it = g_activeFlights.find(record.planeId);
    if (it == g_activeFlights.end())
    {
        FlightState state;
        InitializeFlightState(state, record.planeId);
        g_activeFlights[record.planeId] = state;

        cout << "[Serialization] Warning: telemetry received before FLIGHT_START for plane "
            << record.planeId << ". Flight state auto-created.\n";
    }

    // Forward valid telemetry to flight calculation in real time
    AddTelemetrySample(g_activeFlights[record.planeId], record.timestamp, record.fuel);

    /*cout << "[Serialization] Telemetry accepted for plane "
        << record.planeId
        << " | Fuel: " << record.fuel
        << " | Time: " << record.timestamp
        << "\n";*/

    return true;
}

bool HandleFlightEnd(uint32_t planeId)
{
    if (!ValidatePlaneId(planeId))
    {
        cout << "[Serialization] Invalid plane ID on FLIGHT_END: " << planeId << "\n";
        return false;
    }

    auto it = g_activeFlights.find(planeId);
    if (it == g_activeFlights.end())
    {
        cout << "[Serialization] No active flight state found for plane "
            << planeId << " on FLIGHT_END.\n";
        return false;
    }

    cout << "[Serialization] Finalizing flight for plane " << planeId << "\n";
    PrintFlightSummary(it->second);

    g_activeFlights.erase(it);
    return true;
}

void clientHandler(SOCKET clientSocket)
{
    cout << "Client connected!\n";

    PacketHeader header;

    while (true)
    {
        int result = recvAll(clientSocket, (char*)&header, sizeof(PacketHeader));

        if (result <= 0)
        {
            cout << "Client disconnected or error.\n";
            break;
        }

        if (header.msgType != MSG_FLIGHT_START &&
            header.msgType != MSG_TELEMETRY &&
            header.msgType != MSG_FLIGHT_END)
        {
            cout << "Invalid message type. Closing connection.\n";
            break;
        }

        switch (header.msgType)
        {
        case MSG_FLIGHT_START:
            cout << "[Plane " << header.planeID << "] FLIGHT START\n";
            HandleFlightStart(header.planeID);
            break;

        case MSG_TELEMETRY:
        {
            HandleTelemetryMessage(clientSocket, header.planeID);
            break;
        }

        case MSG_FLIGHT_END:
            cout << "[Plane " << header.planeID << "] FLIGHT END\n";
            HandleFlightEnd(header.planeID);
            return;

        default:
            cout << "Unknown message type: " << (int)header.msgType << "\n";
            break;
        }
    }

    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}

int main()
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cout << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cout << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server running on port " << PORT << endl;

    while (true)
    {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

        if (clientSocket != INVALID_SOCKET)
        {
            thread(clientHandler, clientSocket).detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}