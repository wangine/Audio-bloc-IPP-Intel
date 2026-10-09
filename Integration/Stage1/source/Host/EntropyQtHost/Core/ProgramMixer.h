#pragma once

#include <cstdint>

namespace entropy::host {

struct ProgramMixTelemetry
{
    double peakLeft = 0.0;
    double peakRight = 0.0;
    std::uint32_t overloadMask = 0;
    std::uint32_t frames = 0;
};

class ProgramMixer final
{
public:
    static bool process(const double* musicLeft,
                        const double* musicRight,
                        const double* auxLeft,
                        const double* auxRight,
                        double* programLeft,
                        double* programRight,
                        std::uint32_t frames,
                        ProgramMixTelemetry& telemetry) noexcept;
};

} // namespace entropy::host
