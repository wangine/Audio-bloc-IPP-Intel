#pragma once

#include <entropy/final_guardian_abi.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <vector>

namespace entropy::guardian
{
class FinalGuardianEngine
{
public:
    bool configure(const EntropyFinalGuardianConfig& requested);
    void reset(bool preserveInterventionCount = false) noexcept;
    EntropyResult process(const EntropyAudioBlockF64& block,
                          const EntropyProcessContext* context) noexcept;
    bool configured() const noexcept { return configured_; }
    const EntropyFinalGuardianConfig& config() const noexcept { return config_; }
    EntropyFinalGuardianTelemetry telemetry() const noexcept;
    std::uint32_t latencyFrames() const noexcept { return lookaheadFrames_; }

private:
    static constexpr int kTaps = 256;
    static constexpr int kPhases = ENTROPY_FINAL_GUARDIAN_OVERSAMPLE_FACTOR;
    static constexpr int kFirstOffset = -(kTaps / 2 - 1);
    static constexpr int kLastOffset = kTaps / 2;

    static std::uint32_t nextPowerOfTwo(std::uint32_t value) noexcept;
    bool buildDetector();
    double sample(std::uint32_t channel, std::int64_t frame) const noexcept;
    double reconstruct(std::uint32_t channel, std::int64_t frame,
                       int phase) const noexcept;
    void writeInput(const EntropyAudioBlockF64& block) noexcept;
    void detectAvailable() noexcept;
    void latchTrip(std::int64_t frame, std::uint32_t channel, double peak) noexcept;
    double safetyGain(std::int64_t outputFrame) noexcept;
    void silence(const EntropyAudioBlockF64& block) noexcept;

    EntropyFinalGuardianConfig config_ {};
    std::vector<double> ring_;
    std::vector<double> coefficients_;
    std::vector<double> coefficientsPhase4_;
    bool useAvx2_ = false;
    double maximumKernelL1_ = 1.0;
    std::uint32_t capacityFrames_ = 0;
    std::uint32_t lookaheadFrames_ = 0;
    std::uint32_t fadeFrames_ = 0;
    double ceilingLinear_ = 1.0;
    std::int64_t writeFrame_ = 0;
    std::int64_t nextDetectFrame_ = 0;
    std::int64_t tripFrame_ = (std::numeric_limits<std::int64_t>::max)();
    bool configured_ = false;

    std::atomic<std::uint32_t> state_ {ENTROPY_GUARDIAN_ARMED};
    std::atomic<std::uint32_t> faultFlags_ {ENTROPY_GUARDIAN_FAULT_NONE};
    std::atomic<std::uint32_t> tripChannel_ {0};
    std::atomic<double> maximumObservedPeak_ {0.0};
    std::atomic<double> tripPeak_ {0.0};
    std::atomic<std::uint64_t> tripInputFrame_ {0};
    std::atomic<std::uint64_t> processedInputFrames_ {0};
    std::atomic<std::uint64_t> producedOutputFrames_ {0};
    std::atomic<std::uint64_t> interventionCount_ {0};
    std::atomic<std::uint64_t> nonfiniteCount_ {0};
};
} // namespace entropy::guardian
