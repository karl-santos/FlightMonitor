#include "Flightcalc.h"
#include <cstdio>
#include <iostream>
#include <iomanip>

/// @file flightcalc.cpp
/// @brief Implements flight state tracking and summary reporting for incoming telemetry data.

/// @brief Initializes a FlightState to a clean starting state.
/// @param state   The FlightState to initialize.
/// @param planeId The unique ID of the plane this state belongs to.
void InitializeFlightState(FlightState& state, uint32_t planeId) {
    state.planeId = planeId;
    state.hasFirstSample = false;
    state.firstTimestamp = 0;
    state.lastTimestamp = 0;
    state.firstFuel = 0.0f;
    state.lastFuel = 0.0f;
    state.sampleCount = 0;
}

/// @brief Adds a telemetry sample to an active flight state.
///
/// Updates the first and last timestamps and fuel values as samples arrive.
/// @param state     The FlightState to update.
/// @param timestamp Unix epoch timestamp (UTC) of the sample.
/// @param fuel      Fuel remaining in gallons at the time of the sample.
void AddTelemetrySample(FlightState& state, uint64_t timestamp, float fuel) {
    if (!state.hasFirstSample) {
        state.firstTimestamp = timestamp;
        state.firstFuel = fuel;
        state.lastTimestamp = timestamp;
        state.lastFuel = fuel;
        state.hasFirstSample = true;
    }
    else {
        if (timestamp < state.firstTimestamp) state.firstTimestamp = timestamp;
        if (timestamp > state.lastTimestamp) {
            state.lastTimestamp = timestamp;
            state.lastFuel = fuel;
        }
    }
    state.sampleCount++;
}

/// @brief Prints a summary of the completed flight to standard output.
///
/// Calculates and displays total flight duration, fuel used, and average
/// fuel consumption per hour. Requires at least 2 samples to produce a result.
/// @param state The FlightState containing the accumulated flight data.
void PrintFlightSummary(const FlightState& state) {
    if (!state.hasFirstSample || state.sampleCount < 2) {
        std::cout << "[Plane " << state.planeId << "] Not enough data to compute flight metrics (samples=" << state.sampleCount << ").\n";
        return;
    }

    double totalSeconds = (state.lastTimestamp - state.firstTimestamp);
    double hours = totalSeconds / 3600.0;
    if (hours <= 0.0) {
        std::cout << "[Plane " << state.planeId << "] Invalid flight duration: " << totalSeconds << " seconds.\n";
        return;
    }

    double totalFuelUsed = (state.firstFuel - state.lastFuel);
    double averageConsumptionPerHour = totalFuelUsed / hours;

    std::cout << "=== Flight summary for plane " << state.planeId << " ===\n";
    std::cout << "Samples: " << state.sampleCount << "\n";
    std::cout << "Start timestamp: (In seconds since epoch UTC) " << state.firstTimestamp << "\n";
    std::cout << "End timestamp: (In seconds since epoch UTC) " << state.lastTimestamp << "\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Total flight time: " << totalSeconds << " seconds (" << hours << " hours)\n";
    std::cout << std::setprecision(6);
    std::cout << "Start fuel: " << state.firstFuel << "\n";
    std::cout << "End fuel:   " << state.lastFuel << "\n";
    std::cout << "Total fuel used: " << totalFuelUsed << "\n";
    std::cout << "Average consumption (fuel units per hour): " << averageConsumptionPerHour << "\n";
    std::cout << "=====================================\n";
}