#pragma once

#include <array>

namespace entropy::host
{
struct ProgramRoutePointers
{
    std::array<const double*, 2> centerInputs{};
    std::array<const double*, 3> crossoverMusicInputs{};
};

class ProgramRouteFanout final
{
public:
    static ProgramRoutePointers bind(
        const double* rawLeft, const double* rawRight,
        const double* processedLeft, const double* processedRight,
        const double* centerCompatibilityLeft,
        const double* centerCompatibilityRight,
        const double* centerOnly) noexcept;
};
} // namespace entropy::host
