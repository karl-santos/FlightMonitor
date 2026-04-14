#pragma once
#include <cstdint>

/// @file flightcalc.h
/// @brief Declares the FlightState structure and functions for tracking and summarizing flight telemetry.

/// @brief Holds accumulated telemetry data for a single plane during a flight.
struct FlightState {
    uint32_t planeId;        ///< Unique identifier for the plane.
    bool hasFirstSample;     ///< True once at least one telemetry sample has been received.
    uint64_t firstTimestamp; ///< Timestamp of the earliest received sample (Unix epoch, UTC).
    uint64_t lastTimestamp;  ///< Timestamp of the most recent received sample (Unix epoch, UTC).
    float firstFuel;         ///< Fuel level recorded at the first sample.
    float lastFuel;          ///< Fuel level recorded at the latest sample.
    int sampleCount;         ///< Total number of telemetry samples received.
};

/// @brief Initializes a FlightState to a clean starting state.
/// @param state   The FlightState to initialize.
/// @param planeId The unique ID of the plane this state belongs to.
void InitializeFlightState(FlightState& state, uint32_t planeId);

/// @brief Adds a telemetry sample to an active flight state.
///
/// Updates the first and last timestamps and fuel values as samples arrive.
/// @param state     The FlightState to update.
/// @param timestamp Unix epoch timestamp (UTC) of the sample.
/// @param fuel      Fuel remaining in gallons at the time of the sample.
void AddTelemetrySample(FlightState& state, uint64_t timestamp, float fuel);

/// @brief Prints a summary of the completed flight to standard output.
///
/// Calculates and displays total flight duration, fuel used, and average
/// fuel consumption per hour. Requires at least 2 samples to produce a result.
/// @param state The FlightState containing the accumulated flight data.
void PrintFlightSummary(const FlightState& state);