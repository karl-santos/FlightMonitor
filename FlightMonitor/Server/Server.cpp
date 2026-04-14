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

/// @file Server.cpp
/// @brief Implements the TCP server that receives flight telemetry from multiple clients concurrently.
///
/// Each connecting client is handled on its own thread. The server processes
/// FLIGHT_START, FLIGHT_TELEMETRY, and FLIGHT_END packets, maintaining a
/// FlightState per plane and printing a summary when a flight concludes.

using namespace std;

/// @brief Internal record used to pass validated telemetry data between server modules.
struct TelemetryRecord
{
    uint32_t planeId;   ///< Unique identifier of the plane.
    uint64_t timestamp; ///< Unix epoch timestamp (UTC) of the telemetry reading.
    float fuel;         ///< Fuel remaining in gallons at the time of the reading.
};

/// @brief Map of active flight states, keyed by plane ID.
static unordered_map<uint32_t, FlightState> g_activeFlights;

/// @brief Receives exactly the requested number of bytes from a socket.
/// @param sock   The socket to receive from.
/// @param buffer Destination buffer for the received data.
/// @param size   Number of bytes to receive.
/// @return Number of bytes received, 0 on disconnect, or negative on error.
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

/// @brief Validates that a plane ID is non-zero.
/// @param planeId The plane ID to validate.
/// @return True if valid, false otherwise.
static bool ValidatePlaneId(uint32_t planeId)
{
    return planeId != 0;
}

/// @brief Validates that a timestamp is non-zero.
/// @param timestamp The timestamp to validate.
/// @return True if valid, false otherwise.
static bool ValidateTimestamp(uint64_t timestamp)
{
    return timestamp != 0;
}

/// @brief Validates that a fuel value is within an acceptable range.
/// @param fuel The fuel value to validate (gallons).
/// @return True if fuel is between 0.0 and 100000.0, false otherwise.
static bool ValidateFuel(float fuel)
{
    return fuel >= 0.0f && fuel <= 100000.0f;
}

/// @brief Builds and validates an internal TelemetryRecord from a raw payload.
/// @param planeId The plane ID from the packet header.
/// @param payload The raw telemetry payload received from the client.
/// @param record  Output record populated with validated telemetry data.
/// @return True if the record passed all validation checks, false otherwise.
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

/// @brief Handles a FLIGHT_START packet by initializing a new flight state for the plane.
/// @param planeId The unique ID of the plane starting its flight.
/// @return True if the flight state was successfully initialized, false if the plane ID is invalid.
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

/// @brief Handles a MSG_TELEMETRY packet by reading the payload, validating it, and updating the flight state.
///
/// If telemetry arrives before a FLIGHT_START for that plane, a flight state is auto-created.
/// @param clientSocket The socket to read the telemetry payload from.
/// @param planeId      The plane ID from the already-read packet header.
/// @return True if the telemetry was successfully received and recorded, false otherwise.
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

    return true;
}

/// @brief Handles a FLIGHT_END packet by printing the flight summary and removing the flight state.
/// @param planeId The unique ID of the plane ending its flight.
/// @return True if the flight was successfully finalized, false if no active state was found.
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

/// @brief Entry point for each client handler thread.
///
/// Reads packet headers in a loop and dispatches to the appropriate handler
/// based on message type. Closes the socket when the client disconnects or
/// sends an invalid message type.
/// @param clientSocket The accepted client socket to handle.
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
            HandleTelemetryMessage(clientSocket, header.planeID);
            break;

        case MSG_FLIGHT_END:
            cout << "[Plane " << header.planeID << "] FLIGHT END\n";
            HandleFlightEnd(header.planeID);
            break;

        default:
            cout << "Unknown message type: " << (int)header.msgType << "\n";
            break;
        }
    }

    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}

/// @brief Server entry point. Initializes Winsock, binds to PORT, and accepts client connections.
///
/// Each accepted client is dispatched to clientHandler() on a detached thread,
/// allowing unlimited concurrent client connections.
/// @return 0 on clean exit, 1 on initialization failure.
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