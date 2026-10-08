#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#ifndef ENTROPY_EGRESS_FORENSIC_TRACE
#define ENTROPY_EGRESS_FORENSIC_TRACE 0
#endif
#if ENTROPY_EGRESS_FORENSIC_TRACE
#include <entropy/egress_forensic_abi.h>
#endif

namespace entropy::egress
{
struct ClockObservation
{
    std::uint64_t sinkFrame = 0;
    std::uint64_t host100ns = 0;
    std::uint64_t nominalPublished = 0;
    std::uint32_t renderFrames = 0;
    double fillBefore = 0.0;
    bool continuous = false;
};

struct ClockEstimate
{
    double sinkFramesPerSecond = 0.0;
    double effectiveNominalFramesPerSecond = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

class ClockEstimator
{
public:
    bool prepare(std::uint32_t sinkRate) noexcept;
    void reset() noexcept;
    ClockEstimate observe(const ClockObservation& observation) noexcept;
#if ENTROPY_EGRESS_FORENSIC_TRACE
    // B2-FF-007: owner-thread snapshot; NEVER read from the producer/UI thread.
    EntropyForensicControl forensic() const noexcept { return forensic_; }
#endif

private:
#if ENTROPY_EGRESS_FORENSIC_TRACE
    EntropyForensicControl forensic_ {};
    void rejectForensic(std::uint32_t reason) noexcept {
        forensic_.estimate_reason=reason;
        ++forensic_.consecutive_rejections;
    }
#endif
    struct Sample
    {
        std::uint64_t sinkFrame = 0;
        std::uint64_t host100ns = 0;
        std::uint64_t nominalPublished = 0;
    };

    static constexpr std::size_t kCapacity = 8192;
    static constexpr double kWindowSeconds = 4.0;

    std::array<Sample, kCapacity> samples_ {};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
    std::uint32_t sinkRate_ = 0;
    std::uint64_t lastSinkFrame_ = 0;
    std::uint64_t lastHost100ns_ = 0;
    std::uint64_t lastNominalPublished_ = 0;
    bool haveLast_ = false;
};
} // namespace entropy::egress
