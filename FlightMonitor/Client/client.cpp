#include "client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>


Client::Client(const std::string& serverIP, int port,
    uint32_t planeID, const std::string& dataFile)
    : serverIP_(serverIP), port_(port),
    planeID_(planeID), dataFile_(dataFile),
    sock_(INVALID_SOCKET)
{
}

Client::~Client() {
    disconnect();
}


// Startup module
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


// File reader module
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
    }

    file.close();
    return true;
}



// Helper: parse "12_3_2023 14:56:47" -> unix epoch uint64_t
uint64_t Client::parseTimestamp(const std::string& ts) {
    // format: DD_M_YYYY HH:MM:SS
    int day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;
    sscanf_s(ts.c_str(), "%d_%d_%d %d:%d:%d",
        &day, &month, &year, &hour, &min, &sec);

    struct tm t {};
    t.tm_mday = day;
    t.tm_mon = month - 1;  // tm_mon is 0-indexed
    t.tm_year = year - 1900;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    t.tm_isdst = -1;

    return static_cast<uint64_t>(mktime(&t));
}



// Packetizer module
TelemetryPacket Client::buildTelemetryPacket(uint64_t timestamp, float fuel) {
    TelemetryPacket pkt{};
    pkt.header.length = sizeof(TelemetryPacket);
    pkt.header.msgType = MSG_TELEMETRY;
    pkt.header.planeID = planeID_;
    pkt.payload.timestamp = timestamp;
    pkt.payload.fuel = fuel;
    return pkt;
}

PacketHeader Client::buildControlPacket(uint8_t msgType) {
    PacketHeader hdr{};
    hdr.length = sizeof(PacketHeader);
    hdr.msgType = msgType;
    hdr.planeID = planeID_;
    return hdr;
}


// Transmitter module
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

void Client::disconnect() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        WSACleanup();
    }
}