#pragma once
#include <winsock2.h>
#include <cstdint>

bool HandleFlightStart(uint32_t planeId);
bool HandleTelemetryMessage(SOCKET clientSocket, uint32_t planeId);
bool HandleFlightEnd(uint32_t planeId);