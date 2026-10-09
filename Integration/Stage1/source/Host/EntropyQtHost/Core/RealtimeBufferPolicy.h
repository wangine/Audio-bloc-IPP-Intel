#pragma once

#include <algorithm>
#include <cstdint>

namespace entropy::host
{
// Central host-side latency policy. These are transport/clock cushions only;
// they do not resample, alter samples, or change any module ABI.
struct RealtimeBufferPolicy final
{
    static constexpr std::uint32_t sourceCapacityFrames = 524288;
    static constexpr std::uint32_t sourcePrerollFrames = 8192;
    static constexpr std::uint32_t sourceTargetFrames = 16384;
    static constexpr std::uint32_t sourceCaptureCapacityFrames = 131072;

    static constexpr std::uint32_t egressCapacityFrames = 65536;
    // The accepted A05a target is a duration: 16384 / 192000 seconds.
    // Keep the rational form so every sink rate receives the same duration
    // without floating-point rounding in configuration code.
    static constexpr std::uint32_t egressReferenceRate = 192000;
    static constexpr std::uint32_t egressReferenceTargetFrames = 16384;
    static constexpr std::uint32_t egressTargetFrames =
        egressReferenceTargetFrames;
    static constexpr std::uint32_t egressInterpolationGuardFrames = 100;

    static constexpr std::uint32_t egressTargetFramesForRate(
        std::uint32_t sinkRate) noexcept
    {
        if (sinkRate == 0) return 0;
        const auto scaled =
            static_cast<std::uint64_t>(egressReferenceTargetFrames) * sinkRate;
        return static_cast<std::uint32_t>(
            (scaled + egressReferenceRate - 1u) / egressReferenceRate);
    }

    static constexpr std::uint32_t egressStartupPrefillSinkFrames(
        std::uint32_t sinkRate, std::uint32_t maximumRenderFrames) noexcept
    {
        return egressTargetFramesForRate(sinkRate) + maximumRenderFrames +
            egressInterpolationGuardFrames;
    }

    static constexpr std::uint32_t egressMaximumRenderFramesForRate(
        std::uint32_t sinkRate,
        std::uint32_t implementationMaximumFrames) noexcept
    {
        const auto target = egressTargetFramesForRate(sinkRate);
        if (target <= egressInterpolationGuardFrames) return 0;
        return (std::min)(implementationMaximumFrames,
                          target - egressInterpolationGuardFrames);
    }
};
} // namespace entropy::host
