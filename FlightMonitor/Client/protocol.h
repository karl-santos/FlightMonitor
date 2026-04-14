#pragma once
#include <stdint.h>

/// @file protocol.h
/// @brief Defines the binary communication protocol between the flight client and server.
///
/// All packets begin with a PacketHeader. Telemetry packets additionally
/// include a TelemetryPayload. Control packets (FLIGHT_START, FLIGHT_END)
/// consist of the header only.

// ─── Message Type Constants ───────────────────────────────────────────────────

/// @brief Message type identifier for a flight start control packet.
#define MSG_FLIGHT_START  0x01

/// @brief Message type identifier for a telemetry data packet.
#define MSG_TELEMETRY     0x02

/// @brief Message type identifier for a flight end control packet.
#define MSG_FLIGHT_END    0x03

#pragma pack(push, 1)

// ─── Packet Structures ────────────────────────────────────────────────────────

/// @brief Common header present in every packet (7 bytes).
///
/// The server reads this header first to determine the message type
/// and the originating plane before reading any additional payload.
typedef struct {
    uint16_t length;    ///< Total packet size in bytes, including header and payload.
    uint8_t  msgType;   ///< Message type: MSG_FLIGHT_START, MSG_TELEMETRY, or MSG_FLIGHT_END.
    uint32_t planeID;   ///< Unique identifier for the plane (set to the client's process ID).
} PacketHeader;

/// @brief Payload carried by telemetry packets (12 bytes).
typedef struct {
    uint64_t timestamp; ///< Unix epoch timestamp in seconds (UTC) of the telemetry reading.
    float    fuel;      ///< Fuel remaining onboard in gallons at the time of the reading.
} TelemetryPayload;

/// @brief Complete telemetry packet sent from client to server (19 bytes total).
///
/// Combines the common PacketHeader with a TelemetryPayload.
/// The server identifies this packet by checking header.msgType == MSG_TELEMETRY.
typedef struct {
    PacketHeader     header;  ///< Common packet header.
    TelemetryPayload payload; ///< Telemetry data payload.
} TelemetryPacket;

#pragma pack(pop)

// ─── Network Configuration ────────────────────────────────────────────────────

/// @brief Default TCP port the server listens on and clients connect to.
#define PORT 6767