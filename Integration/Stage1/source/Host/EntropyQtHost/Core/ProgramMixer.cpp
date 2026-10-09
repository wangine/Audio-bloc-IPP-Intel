#include "ProgramMixer.h"

#include <algorithm>
#include <cmath>

namespace entropy::host {

bool ProgramMixer::process(const double* musicLeft,
                           const double* musicRight,
                           const double* auxLeft,
                           const double* auxRight,
                           double* programLeft,
                           double* programRight,
                           std::uint32_t frames,
                           ProgramMixTelemetry& telemetry) noexcept
{
    telemetry = {};
    if (musicLeft == nullptr || musicRight == nullptr ||
        auxLeft == nullptr || auxRight == nullptr ||
        programLeft == nullptr || programRight == nullptr ||
        frames == 0)
        return false;

    double peakLeft = 0.0;
    double peakRight = 0.0;

    for (std::uint32_t frame = 0; frame < frames; ++frame)
    {
        const double left = musicLeft[frame] + auxLeft[frame];
        const double right = musicRight[frame] + auxRight[frame];

        programLeft[frame] = left;
        programRight[frame] = right;

        peakLeft = (std::max)(peakLeft, std::abs(left));
        peakRight = (std::max)(peakRight, std::abs(right));
    }

    telemetry.peakLeft = peakLeft;
    telemetry.peakRight = peakRight;
    telemetry.frames = frames;
    if (peakLeft > 1.0) telemetry.overloadMask |= 1u;
    if (peakRight > 1.0) telemetry.overloadMask |= 2u;
    return true;
}

} // namespace entropy::host
