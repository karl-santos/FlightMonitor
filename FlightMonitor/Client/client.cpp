#include "client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>

/// @file client.cpp
/// @brief Implements the Client class: connection, file reading, packetizing, and transmission.

#pragma comment(lib, "ws2_32.lib")

/// @brief Constructs a Client with the given connection and flight parameters.
/// @param serverIP  IP address of the server to connect to.
/// @param port      TCP port the server is listening on.
/// @param planeID   Unique identifier for this plane (typically the process ID).
/// @param dataFile  Path to the telemetry CSV file to stream.
Client::Client(const std::string& serverIP, int port,
    uint32_t planeID, const std::string& dataFile)
    : serverIP_(serverIP), port_(port),
    planeID_(planeID), dataFile_(dataFile),
    sock_(INVALID_SOCKET)
{
}

/// @brief Destructor. Closes the socket if still open.
Client::~Client() {
    disconnect();
}

/// @brief Runs the full flight session: connect, stream telemetry, disconnect.
/// @return True if the session completed successfully, false on any error.
bool Client::run() {
    if (!connectToServer()) return false;

    // Send FLIGHT_START header-only packet
    PacketHeader startPkt = buildControlPacket(MSG_FLIGHT_START);
    if (!transmit(&startPkt, sizeof(startPkt))) {
        std::cerr << "[" << planeID_ << "] Failed to send FLIGHT_START\n";
        return false;
    }

    // Run the file reader / packetizer / transmitter loop
    runFlightLoop();

    // Send FLIGHT_END header-only packet
    PacketHeader endPkt = buildControlPacket(MSG_FLIGHT_END);
    transmit(&endPkt, sizeof(endPkt));

    disconnect();
    std::cout << "[" << planeID_ << "] Flight complete. Disconnected.\n";
    return true;
}

/// @brief Establishes a TCP connection to the server.
/// @return True if the connection was successful, false otherwise.
bool Client::connectToServer() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[" << planeID_ << "] WSAStartup failed\n";
        return false;
    }

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "[" << planeID_ << "] socket() failed\n";
        WSACleanup();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port_));
    inet_pton(AF_INET, serverIP_.c_str(), &addr.sin_addr);

    if (connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[" << planeID_ << "] connect() failed\n";
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    std::cout << "[" << planeID_ << "] Connected to " << serverIP_ << ":" << port_ << "\n";
    return true;
}

/// @brief Reads the telemetry file line by line and transmits each reading.
///
/// Parses each line for a timestamp and fuel value, builds a telemetry
/// packet, and transmits it. Sleeps 5ms between packets.
/// @return True if the file was read to completion, false on a socket error.
bool Client::runFlightLoop() {
    std::ifstream file(dataFile_);
    if (!file.is_open()) {
        std::cerr << "[" << planeID_ << "] Cannot open file: " << dataFile_ << "\n";
        return false;
    }

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        // Strip the "FUEL TOTAL QUANTITY," label from the first line
        if (firstLine) {
            firstLine = false;
            size_t comma = line.find(',');
            if (comma == std::string::npos) continue;
            line = line.substr(comma + 1);
        }

        // Trim leading whitespace and carriage returns
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Parse:  "DD_M_YYYY HH:MM:SS,47.865124,"
        std::istringstream ss(line);
        std::string tsStr, fuelStr;
        if (!std::getline(ss, tsStr, ',')) continue;
        if (!std::getline(ss, fuelStr, ',')) continue;

        // Trim trailing whitespace
        while (!fuelStr.empty() && (fuelStr.back() == ' ' ||
            fuelStr.back() == '\r' || fuelStr.back() == '\n'))
            fuelStr.pop_back();

        float fuel = 0.0f;
        try { fuel = std::stof(fuelStr); }
        catch (...) { continue; }

        uint64_t ts = parseTimestamp(tsStr);

        // Pass to packetizer then transmitter
        TelemetryPacket pkt = buildTelemetryPacket(ts, fuel);
        if (!transmit(&pkt, sizeof(pkt))) {
            std::cerr << "[" << planeID_ << "] Socket error during transmit\n";
            break;
        }
        std::this_thread::sleep_for(5ms);
    }

    file.close();
    return true;
}

/// @brief Parses a timestamp string into a Unix epoch value.
/// @param ts Timestamp string in "DD_M_YYYY HH:MM:SS" format.
/// @return Unix epoch time in seconds as a uint64_t.
uint64_t Client::parseTimestamp(const std::string& ts) {
    int day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;
    sscanf_s(ts.c_str(), "%d_%d_%d %d:%d:%d",
        &day, &month, &year, &hour, &min, &sec);

    struct tm t {};
    t.tm_mday = day;
    t.tm_mon = month - 1;
    t.tm_year = year - 1900;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    t.tm_isdst = -1;

    return static_cast<uint64_t>(mktime(&t));
}

/// @brief Builds a complete telemetry packet from a timestamp and fuel value.
/// @param timestamp Unix epoch timestamp (UTC) of the reading.
/// @param fuel      Fuel remaining in gallons.
/// @return A fully populated TelemetryPacket ready to transmit.
TelemetryPacket Client::buildTelemetryPacket(uint64_t timestamp, float fuel) {
    TelemetryPacket pkt{};
    pkt.header.length = sizeof(TelemetryPacket);
    pkt.header.msgType = MSG_TELEMETRY;
    pkt.header.planeID = planeID_;
    pkt.payload.timestamp = timestamp;
    pkt.payload.fuel = fuel;
    return pkt;
}

/// @brief Builds a control packet (FLIGHT_START or FLIGHT_END) with no payload.
/// @param msgType The message type: MSG_FLIGHT_START or MSG_FLIGHT_END.
/// @return A fully populated PacketHeader ready to transmit.
PacketHeader Client::buildControlPacket(uint8_t msgType) {
    PacketHeader hdr{};
    hdr.length = sizeof(PacketHeader);
    hdr.msgType = msgType;
    hdr.planeID = planeID_;
    return hdr;
}

/// @brief Transmits raw bytes over the TCP socket, retrying until all bytes are sent.
/// @param data Pointer to the data buffer to send.
/// @param size Number of bytes to send.
/// @return True if all bytes were sent successfully, false on a socket error.
bool Client::transmit(const void* data, int size) {
    const char* ptr = reinterpret_cast<const char*>(data);
    int remaining = size;
    while (remaining > 0) {
        int sent = send(sock_, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) return false;
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

/// @brief Closes the TCP socket and cleans up Winsock.
void Client::disconnect() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        WSACleanup();
    }
}