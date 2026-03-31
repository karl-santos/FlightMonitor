#pragma once
#include <stdint.h>

// Message type values
#define MSG_FLIGHT_START  0x01
#define MSG_TELEMETRY     0x02
#define MSG_FLIGHT_END    0x03

#pragma pack(push, 1)


// Header - present in ALL packets (7 bytes total)

typedef struct {
    uint16_t length;       // total packet size in bytes
    uint8_t  msgType;      // MSG_FLIGHT_START, MSG_TELEMETRY, or MSG_FLIGHT_END
    uint32_t planeID;      // unique plane ID
} PacketHeader;


// Telemetry payload (12 bytes)
typedef struct {
    uint64_t timestamp;    // unix epoch seconds
    float    fuel;         // gallons remaining
} TelemetryPayload;


#pragma pack(pop)

#define PORT 6767