#pragma once

#include <array>
#include <atomic>
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
    struct Quality
    {
        double shortWindowHz=0, longWindowHz=0, filteredHz=0;
        double windowSeconds=0, bucketResidualRmsFrames=0;
        std::uint64_t estimates=0, accepted=0, rejected=0;
        std::uint64_t observationFrame=0, observationHost100ns=0;
        bool valid=false, coherent=false;
    };
    bool prepare(std::uint32_t sinkRate) noexcept;
    void reset() noexcept;
    ClockEstimate observe(const ClockObservation& observation) noexcept;
    Quality quality() const noexcept; // bounded, passive, thread-safe observation
#if ENTROPY_EGRESS_FORENSIC_TRACE
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
    // Integer anchors are subtracted BEFORE FP conversion, including on MSVC
    // where long double is FP64. Offsets are means relative to each anchor.
    struct Sample
    {
        std::uint64_t sinkFrame=0, host100ns=0, nominalPublished=0;
        double sinkOffset=0, hostOffset=0, nominalOffset=0;
    };
    struct Fit
    {
        double sinkHz=0, nominalHz=0, span=0, residualRms=0;
        bool valid=false;
    };
    static constexpr std::size_t kCapacity=512;
    static constexpr double kAcquireSeconds=3.9;
    static constexpr double kShortSeconds=4.0;
    static constexpr double kWindowSeconds=32.0;
    static constexpr double kFilterSeconds=4.0;
    static constexpr std::uint64_t kBucketTicks=1000000; // 100 ms; no audio buffering
    Fit fit(double windowSeconds) const noexcept;
    void clearWindow() noexcept;
    void publishQuality(const Fit& shortFit, const Fit& longFit) noexcept;
    ClockEstimate cachedResult() noexcept;
    std::array<Sample,kCapacity> samples_{};
    Sample bucket_{};
    std::uint32_t bucketCount_=0, sinkRate_=0;
    long double bucketSinkSum_=0, bucketHostSum_=0, bucketNominalSum_=0;
    std::size_t next_=0, count_=0;
    std::uint64_t lastSinkFrame_=0, lastHost100ns_=0, lastNominalPublished_=0;
    std::uint64_t lastFitHost_=0, estimates_=0;
    bool haveLast_=false;
    ClockEstimate cached_{};
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> qualitySequence_{0}, accepted_{0}, rejected_{0}, fits_{0};
    std::atomic<std::uint64_t> observationFrame_{0}, observationHost_{0};
    std::atomic<double> shortHz_{0}, longHz_{0}, filteredHz_{0}, span_{0}, residual_{0};
    std::atomic<bool> estimateValid_{false};
};
} // namespace entropy::egress
