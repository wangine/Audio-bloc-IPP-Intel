#pragma once

#include <entropy/pcm24/Pcm24Converter.h>

#include <array>
#include <atomic>

namespace entropy::pcm24
{
class Pcm24ModuleEngine
{
public:
    bool configure(const EntropyPcm24Config& requested) noexcept;
    void reset() noexcept;
    EntropyResult process(EntropyPcm24BlockF64& block) noexcept;
    EntropyResult clearFault() noexcept;
    bool configured() const noexcept { return configured_; }
    const EntropyPcm24Config& config() const noexcept { return config_; }
    EntropyPcm24Telemetry telemetry() const noexcept;
    std::uint32_t bytesPerSample() const noexcept;

private:
    void seedAll() noexcept;
    void zeroOutput(EntropyPcm24BlockF64& block, std::uint32_t bytes) noexcept;

    EntropyPcm24Config config_ {};
    std::array<DitherState, ENTROPY_PCM24_MAX_CHANNELS> dither_ {};
    bool configured_ = false;
    std::atomic<std::uint32_t> faultFlags_ {ENTROPY_PCM24_FAULT_NONE};
    std::atomic<std::uint64_t> processedFrames_ {0};
    std::atomic<std::uint64_t> overrangeCount_ {0};
    std::atomic<std::uint64_t> nonfiniteCount_ {0};
    std::atomic<std::uint64_t> railClampCount_ {0};
    std::atomic<std::uint64_t> rawFaultBlockCount_ {0};
    std::atomic<std::uint64_t> clipBlockCount_ {0};
};
} // namespace entropy::pcm24

