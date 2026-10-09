#pragma once

#include "ModuleRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host
{
struct SyntheticProbeCheck
{
    std::string name;
    bool passed = false;
    std::string detail;
};

struct SyntheticProbeReport
{
    bool passed = false;
    bool hardwareUntouched = true;
    std::uint32_t processedFrames = 0;
    std::uint32_t pcmBytes = 0;
    std::uint32_t musicPathLatencyFrames = 0;
    double maximumAbsoluteSample = 0.0;
    std::vector<SyntheticProbeCheck> checks;
    std::string error;
};

class SyntheticGraphProbe final
{
public:
    static SyntheticProbeReport run(const ModuleRegistry& registry);
};
} // namespace entropy::host
