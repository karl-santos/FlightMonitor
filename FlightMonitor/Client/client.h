#pragma once


#include "protocol.h"
#include <stdio.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>


using namespace std;


class Client {
public:


    Client(const std::string& serverIP, int port, uint32_t planeID, const std::string& dataFile);
    ~Client();

	bool run();


private:
    string serverIP_;
    int         port_;
    uint32_t    planeID_;
    string dataFile_;
    SOCKET      sock_;

    // Startup module
    bool connectToServer();

    // File reader module
    // Reads one line, parses timestamp+fuel, calls packetizer
    // Returns false when EOF or error
    bool runFlightLoop();

    // Helper: parse "DD_M_YYYY HH:MM:SS" string into unix epoch uint64_t
    uint64_t parseTimestamp(const std::string& ts);

    // Packetizer module
    TelemetryPacket buildTelemetryPacket(uint64_t timestamp, float fuel);
    PacketHeader    buildControlPacket(uint8_t msgType);

    // Transmitter module
    bool transmit(const void* data, int size);

    void disconnect();
};
