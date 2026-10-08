#pragma once

#include <entropy/headroom_abi.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace entropy::headroom
{
class HeadroomEngine
{
public:
    bool prepare(std::uint32_t rate, std::uint32_t maximumFrames) noexcept;
    void reset() noexcept;
    EntropyResult setState(const EntropyHeadroomState&) noexcept;
    EntropyHeadroomState state() const noexcept;
    EntropyResult setMode(std::uint32_t mode) noexcept;
    EntropyResult setPolicy(std::uint32_t mode, double fixedAttenuationDb) noexcept;
    EntropyResult setManualGainDb(double manualGainDb) noexcept;
    EntropyResult setTiming(const EntropyHeadroomTimingStateV5&) noexcept;
    EntropyHeadroomTimingStateV5 timing() const noexcept;
    EntropyHeadroomModeState modeState() const noexcept;
    EntropyHeadroomPolicyStateV2 policyState() const noexcept;
    EntropyHeadroomControlStateV3 controlState() const noexcept;
    EntropyResult clearFault() noexcept;
    EntropyHeadroomTelemetry telemetry() const noexcept;
    EntropyResult process(const EntropyAudioBlockF64&) noexcept;
    EntropyResult analyzeFinal(const EntropyHeadroomAnalysisBlockF64&) noexcept;
    bool prepared() const noexcept { return prepared_; }

private:
    struct Snapshot
    {
        std::uint32_t revision = 0;
        std::uint32_t groupCount = 0;
        std::uint32_t activeMask = 0;
        std::uint32_t transitionSamples = 1;
        std::uint32_t mode = ENTROPY_HEADROOM_MODE_ACTIVE;
        std::uint32_t policyRevision = 0;
        double manualGainDb = -6.0;
        double manualTarget = 0.5011872336272722;
        std::array<std::uint32_t, 16> channelGroup{};
        std::array<double, 16> target{};
        std::array<double, 16> required{};
        std::array<double, 16> margin{};
    };

    struct Ramp
    {
        double current = 1.0;
        double start = 1.0;
        double target = 1.0;
        std::uint32_t remaining = 0;
        std::uint32_t total = 1;
        void begin(double, std::uint32_t) noexcept;
        void force(double) noexcept;
        double next() noexcept;
        double progress() const noexcept;
    };

    bool acquire(Snapshot&) const noexcept;
    void synchronizeAudioState() noexcept;

    std::atomic<std::uint64_t> sequence_{0};
    std::atomic<std::uint64_t> timingSequence_{0};
    std::atomic<double> attackMs_{500.0}, releaseMs_{4000.0};
    double audioAttackMs_=500.0, audioReleaseMs_=4000.0;
    std::atomic<std::uint32_t> revision_{0};
    std::atomic<std::uint32_t> groupCount_{0};
    std::atomic<std::uint32_t> activeMask_{0};
    std::atomic<std::uint32_t> transitionSamples_{1};
    std::atomic<std::uint32_t> requestedMode_{ENTROPY_HEADROOM_MODE_ACTIVE};
    std::atomic<std::uint32_t> policyRevision_{0};
    std::atomic<double> manualGainDb_{-6.0};
    std::atomic<double> manualTarget_{0.5011872336272722};
    std::array<std::atomic<std::uint32_t>, 16> channelGroup_{};
    std::array<std::atomic<double>, 16> target_{}, required_{}, margin_{};
    std::array<std::atomic<double>, 16> positive_{}, inputHeadroom_{}, reserve_{};
    std::array<std::atomic<std::uint32_t>, 16> bounded_{};

    Snapshot audio_{};
    std::array<Ramp, 16> ramps_{};
    std::uint32_t rate_ = 0;
    std::uint32_t maxFrames_ = 0;
    bool prepared_ = false;
    std::atomic<bool> validState_{false};
    std::atomic<std::uint32_t> faults_{ENTROPY_HEADROOM_FAULT_INVALID_CONFIG};
    std::atomic<std::uint32_t> appliedRevision_{0};
    std::atomic<std::uint32_t> effectiveMode_{ENTROPY_HEADROOM_MODE_ACTIVE};
    std::atomic<std::uint32_t> modeTransitionActive_{0};
    std::atomic<double> modeTransitionProgress_{1.0};
    std::atomic<std::uint64_t> nonfinite_{0}, processed_{0};
    std::array<std::atomic<double>, 16> currentTelemetry_{}, progressTelemetry_{};
    std::atomic<double> measuredFinalPeak_{0.0};
    std::array<std::atomic<double>, 16> measuredChannelPeak_{};
    std::atomic<std::uint32_t> limitingChannelMask_{0};
    std::atomic<double> autoAnalysisReductionDb_{0.0};
    std::atomic<std::uint32_t> analyzerState_{ENTROPY_HEADROOM_ANALYZER_IDLE};
    std::array<double, 16> autoTargets_{};
    std::array<std::atomic<double>, 16> autoTargetTelemetry_{};
    bool terminalAnalysis_ = false; // audio-thread owned; no DLL dependency
    std::array<std::uint64_t, 16> autoHoldRemaining_{};
};
} // namespace entropy::headroom
