#pragma once

#include <entropy/output_matrix_abi.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace entropy::output_matrix
{
class OutputMatrixEngine
{
public:
    bool prepare(std::uint32_t sampleRate, std::uint32_t maximumFrames) noexcept;
    void reset() noexcept;
    EntropyResult setState(const EntropyOutputMatrixState& state) noexcept;
    EntropyOutputMatrixState state() const noexcept;
    EntropyOutputMatrixTelemetry telemetry() const noexcept;
    EntropyResult process(const EntropyAudioBlockF64& block) noexcept;
    bool prepared() const noexcept { return prepared_; }

private:
    bool acquireState(EntropyOutputMatrixState& result) const noexcept;

    std::atomic<std::uint64_t> configSequence_ {0};
    std::atomic<std::uint32_t> generation_ {0};
    std::atomic<std::uint32_t> activeSources_ {0};
    std::atomic<std::uint32_t> enabledDestinations_ {0};
    std::array<std::atomic<std::uint32_t>, ENTROPY_OUTPUT_MATRIX_CHANNELS> routes_ {};
    EntropyOutputMatrixState audioState_ {};
    std::uint32_t maximumFrames_ = 0;
    bool prepared_ = false;

    std::atomic<std::uint32_t> appliedGeneration_ {0};
    std::atomic<std::uint32_t> activeDestinations_ {0};
    std::atomic<std::uint32_t> summingDestinations_ {0};
    std::atomic<std::uint32_t> orphanedSources_ {0};
    std::atomic<std::uint32_t> routeCount_ {0};
    std::array<std::atomic<double>, ENTROPY_OUTPUT_MATRIX_CHANNELS> peaks_ {};
    std::atomic<std::uint64_t> processedFrames_ {0};
};
}
