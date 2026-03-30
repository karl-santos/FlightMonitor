#pragma once
#include <cstdint>

struct FlightState {
    uint32_t planeId;
    bool hasFirstSample;
    uint64_t firstTimestamp;
    uint64_t lastTimestamp;
    float firstFuel;
    float lastFuel;
    int sampleCount;
};

void InitializeFlightState(FlightState& state, uint32_t planeId);
void AddTelemetrySample(FlightState& state, uint64_t timestamp, float fuel);
void FinalizeAndPrintFlightSummary(const FlightState& state);