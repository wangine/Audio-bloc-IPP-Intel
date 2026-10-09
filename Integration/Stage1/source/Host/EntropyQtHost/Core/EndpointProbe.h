#pragma once

#include "ModuleRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host
{
struct EndpointProbeInfo
{
    std::string role;
    std::string identifier;
    bool moduleFound = false;
    bool bridgeConnected = false;
    std::uint32_t state = 0;
    std::uint32_t sourceRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t containerBits = 0;
    std::uint32_t validBits = 0;
    std::uint32_t availableFrames = 0;
    std::uint64_t streamGeneration = 0;
    std::uint64_t totalFramesWritten = 0;
    std::uint64_t totalFramesRead = 0;
    std::uint64_t lostFrames = 0;
    std::string detail;
};

struct EndpointProbeReport
{
    std::vector<EndpointProbeInfo> inputs;
    bool nonConsuming = true;
    bool dacUntouched = true;
};

class EndpointProbe final
{
public:
    static EndpointProbeReport run(const ModuleRegistry& registry);
};

const char* endpointStateName(std::uint32_t state) noexcept;
} // namespace entropy::host
