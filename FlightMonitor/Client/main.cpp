#include "client.h"
#include <iostream>
#include <string>

/// @file main.cpp
/// @brief Client application entry point.
///
/// Parses command line arguments and launches a single Client session.
/// Usage: Client.exe <serverIP> <port> <telemetryFile>

using namespace std;

/// @brief Application entry point for the flight telemetry client.
///
/// Expects exactly three command line arguments: the server IP, port, and
/// path to a telemetry data file. The plane ID is derived from the current
/// process ID to ensure uniqueness across simultaneous client instances.
/// @param argc Number of command line arguments (must be at least 4).
/// @param argv Argument vector: argv[1] = serverIP, argv[2] = port, argv[3] = dataFile.
/// @return 0 on success, 1 on argument error or client failure.
int main(int argc, char* argv[])
{
    if (argc < 4) {
        std::cerr << "Usage: Client.exe <serverIP> <port> <dataFile>\n";
        std::cerr << "  e.g: Client.exe 192.168.1.100 54000 Telem_2023_3_12_14_56_40.txt\n";
        return 1;
    }

    string      serverIP = argv[1];
    int         port = std::stoi(argv[2]);
    string      dataFile = argv[3];

    // Use process ID as unique plane ID
    unsigned int planeID = (unsigned int)GetCurrentProcessId();

    std::cout << "[Startup] PlaneID=" << planeID
        << "  Server=" << serverIP << ":" << port
        << "  File=" << dataFile << "\n";

    Client client(serverIP, port, planeID, dataFile);
    return client.run() ? 0 : 1;
}