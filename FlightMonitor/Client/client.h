#pragma once

/// @file client.h
/// @brief Declares the Client class responsible for connecting to the server and streaming flight telemetry.

#include "protocol.h"
#include <stdio.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

/// @brief Manages the full lifecycle of a single flight client session.
///
/// Connects to the server, reads telemetry from a data file line by line,
/// packetizes each reading, and transmits it over TCP. Sends FLIGHT_START
/// and FLIGHT_END control packets to bracket the session.
class Client {
public:

    /// @brief Constructs a Client with the given connection and flight parameters.
    /// @param serverIP  IP address of the server to connect to.
    /// @param port      TCP port the server is listening on.
    /// @param planeID   Unique identifier for this plane (typically the process ID).
    /// @param dataFile  Path to the telemetry CSV file to stream.
    Client(const std::string& serverIP, int port, uint32_t planeID, const std::string& dataFile);

    /// @brief Destructor. Closes the socket if still open.
    ~Client();

    /// @brief Runs the full flight session: connect, stream telemetry, disconnect.
    /// @return True if the session completed successfully, false on any error.
    bool run();

private:
    string      serverIP_;  ///< IP address of the server.
    int         port_;      ///< TCP port of the server.
    uint32_t    planeID_;   ///< Unique plane identifier sent in every packet.
    string      dataFile_;  ///< Path to the telemetry data file.
    SOCKET      sock_;      ///< Active TCP socket handle.

    /// @brief Establishes a TCP connection to the server.
    /// @return True if the connection was successful, false otherwise.
    bool connectToServer();

    /// @brief Reads the telemetry file line by line and transmits each reading.
    ///
    /// Parses each line for a timestamp and fuel value, builds a telemetry
    /// packet, and transmits it. Sleeps 5ms between packets.
    /// @return True if the file was read to completion, false on a socket error.
    bool runFlightLoop();

    /// @brief Parses a timestamp string into a Unix epoch value.
    /// @param ts Timestamp string in "DD_M_YYYY HH:MM:SS" format.
    /// @return Unix epoch time in seconds as a uint64_t.
    uint64_t parseTimestamp(const std::string& ts);

    /// @brief Builds a complete telemetry packet from a timestamp and fuel value.
    /// @param timestamp Unix epoch timestamp (UTC) of the reading.
    /// @param fuel      Fuel remaining in gallons.
    /// @return A fully populated TelemetryPacket ready to transmit.
    TelemetryPacket buildTelemetryPacket(uint64_t timestamp, float fuel);

    /// @brief Builds a control packet (FLIGHT_START or FLIGHT_END) with no payload.
    /// @param msgType The message type: MSG_FLIGHT_START or MSG_FLIGHT_END.
    /// @return A fully populated PacketHeader ready to transmit.
    PacketHeader buildControlPacket(uint8_t msgType);

    /// @brief Transmits raw bytes over the TCP socket, retrying until all bytes are sent.
    /// @param data Pointer to the data buffer to send.
    /// @param size Number of bytes to send.
    /// @return True if all bytes were sent successfully, false on a socket error.
    bool transmit(const void* data, int size);

    /// @brief Closes the TCP socket and cleans up Winsock.
    void disconnect();
};