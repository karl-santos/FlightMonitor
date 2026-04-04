#include "client.h"
#include <iostream>
#include <string>

using namespace std;

// Usage: Client.exe <serverIP> <port> <telemetryFile>
int main(int argc, char* argv[]) 
{

    if (argc < 4) {
        std::cerr << "Usage: Client.exe <serverIP> <port> <dataFile>\n";
        std::cerr << "  e.g: Client.exe 192.168.1.100 54000 Telem_2023_3_12_14_56_40.txt\n";
        return 1;
    }

    string  serverIP = argv[1];
    int          port = std::stoi(argv[2]);
    string  dataFile = argv[3];

    // use process ID as unique plane ID
    unsigned int planeID = (unsigned int)GetCurrentProcessId();

    std::cout << "[Startup] PlaneID=" << planeID
        << "  Server=" << serverIP << ":" << port
        << "  File=" << dataFile << "\n";

    Client client(serverIP, port, planeID, dataFile);
    return client.run() ? 0 : 1;
}