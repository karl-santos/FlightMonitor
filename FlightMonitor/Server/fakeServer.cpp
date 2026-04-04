#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "protocol.h"
#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listenSock, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 5);
    std::cout << "Listening on port " << PORT << "...\n";

    SOCKET client = accept(listenSock, nullptr, nullptr);
    std::cout << "Client connected!\n";

    char buf[256];
    int received;
    while ((received = recv(client, buf, sizeof(buf), 0)) > 0) {
        PacketHeader* hdr = (PacketHeader*)buf;
        if (hdr->msgType == MSG_FLIGHT_START)
            std::cout << "FLIGHT_START  planeID=" << hdr->planeID << "\n";
        else if (hdr->msgType == MSG_FLIGHT_END)
            std::cout << "FLIGHT_END    planeID=" << hdr->planeID << "\n";
        else if (hdr->msgType == MSG_TELEMETRY) {
            TelemetryPacket* pkt = (TelemetryPacket*)buf;
            std::cout << "TELEMETRY     fuel=" << pkt->payload.fuel
                << "  ts=" << pkt->payload.timestamp << "\n";
        }
    }

    std::cout << "Client disconnected.\n";
    closesocket(client);
    closesocket(listenSock);
    WSACleanup();
}