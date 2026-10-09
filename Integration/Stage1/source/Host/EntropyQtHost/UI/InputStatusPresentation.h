#pragma once

#include "Core/EngineTelemetry.h"

namespace entropy::host {
inline const char* describeInputStatus(const EngineInputTelemetry& input,
                                       bool selected) noexcept
{
    if (!input.bridgeConnected) return "DISCONNECTED";
    const bool signal = input.endpointPeak > 1.0e-5;
    if (input.muted) return signal ? "MUTED / SIGNAL ACTIVE" : "MUTED / NO SIGNAL";
    if (!selected) return signal ? "IDLE / SIGNAL ACTIVE" : "IDLE / NO SIGNAL";
    return signal ? "ACTIVE / SIGNAL ACTIVE" : "ACTIVE / NO SIGNAL";
}
}
