#include "client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>



using namespace std;



Client::Client(const std::string& serverIP, int port, int planeID, const std::string& dataFile) : serverIP_(serverIP), port_(port),
    planeID_(planeID), dataFile_(dataFile),
    sock_(INVALID_SOCKET)
{}




// Startup module

bool Client::run() {
    if (!connectToServer()) return false;

    // Send FLIGHT_START header-only packet
    PacketHeader startPkt = buildControlPacket(MSG_FLIGHT_START);

    if (!transmit(&startPkt, sizeof(startPkt))) 
    {
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


bool Client::connectToServer() 
{
    WSADATA wsa;


    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) 
    {
        std::cerr << "[" << planeID_ << "] WSAStartup failed\n";
        return false;
    }

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock_ == SOCKET_ERROR) 
    {
        std::cerr << "[" << planeID_ << "] socket() failed\n";
        WSACleanup();
        return false;
    }

    sockaddr_in SvrAddr{};



    SvrAddr.sin_family = AF_INET;
    SvrAddr.sin_port = htons(port_);
    SvrAddr.sin_addr.s_addr = inet_addr(serverIP_.c_str());

    if (connect(sock_, (struct sockaddr*)&SvrAddr, sizeof(SvrAddr)) == SOCKET_ERROR)
    {
        std::cerr << "[" << planeID_ << "] connect() failed\n";
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    std::cout << "[" << planeID_ << "] Connected to " << serverIP_ << ":" << port_ << "\n";
    return true;
}







