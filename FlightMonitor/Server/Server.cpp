#include <iostream>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "../Client/protocol.h"

using namespace std;

//recieve all bytes
int recvAll(SOCKET sock, char* buffer, int size)
{
    int total = 0;
    //loop until all bytes have been recieved
    while (total < size)
    {
        //recieve data and move pointer forward
        int bytes = recv(sock, buffer + total, size - total, 0);

        if (bytes <= 0)
            return bytes;

        //add and return the amount of bytes that has been read 
        total += bytes;
    }

    return total;
}

//client connection handler
void clientHandler(SOCKET clientSocket)
{
    cout << "Client connected!\n";

    PacketHeader header;
    //read until the client disconnects
    while (true)
    {
        //read the header 
        int result = recvAll(clientSocket, (char*)&header, sizeof(PacketHeader));

        if (result <= 0)
        {
            cout << "Client disconnected or error.\n";
            break;
        }

        //check for invalid header
        if (header.msgType != MSG_FLIGHT_START && header.msgType != MSG_TELEMETRY && header.msgType != MSG_FLIGHT_END)
        {
            cout << "Invalid message type. Closing connection.\n";
            break;
        }

        //Cases for each of the message types
        switch (header.msgType)
        {
            //flight start
        case MSG_FLIGHT_START:
            cout << "[Plane " << header.planeID << "] FLIGHT START\n";
            break;

            //recieving payload - May be changed by serialization module author
        case MSG_TELEMETRY:
        {
            TelemetryPayload payload;
            //read the payload (sets the payload data - the fuel and timestamp)
            int res = recvAll(clientSocket, (char*)&payload, sizeof(payload));

            if (res <= 0)
            {
                cout << "Failed to read telemetry payload\n";
                break;
            }

            //print the telemetry data - not needed in final version
            cout << "[Plane " << header.planeID
                << "] Fuel: " << payload.fuel
                << " | Time: " << payload.timestamp
                << "\n";
            break;
        }
        //flight end
        case MSG_FLIGHT_END:
            //close the connection
            cout << "[Plane " << header.planeID << "] FLIGHT END\n";
            return;

        default:
            cout << "Unknown message type: " << (int)header.msgType << "\n";
            break;
        }
    }

    shutdown(clientSocket, SD_BOTH);
    closesocket(clientSocket);
}


int Main()
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    //create socket
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

    //bind and close socket if it fails
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    //listen and close socket if it fails
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cout << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server running on port " << PORT << endl;

    //run the server forever, until manual closing
    while (true)
    {
        //accept client
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        //give each client their own thread
        if (clientSocket != INVALID_SOCKET)
        {
            thread(clientHandler, clientSocket).detach();
        }
    }
    
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}