#pragma once
#include "BufferPool.h"
#include "Telemetry.h"

#include <PluginProcessor.h>
#include <OutputMatrixEngine.h>
#include <HeadroomEngine.h>
#include <entropy/module_abi.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace entropy::engine {
// Offline deterministic DSP boundary. This is NOT a DAC scheduler.
// Stage2 supplies 13 logical planar FP64 inputs at exactly 192000 Hz.
class AudioGraph final {
public:
    static constexpr std::uint32_t kSampleRate = 192000;
    static constexpr std::uint32_t kInputChannels = 13;
    static constexpr std::uint32_t kOutputChannels = 16;

    // Control thread only, before any audio processing.
    bool prepare(std::uint32_t maximumFrames);
    // Offline control owner only, quiescent with process(). No hardware control.
    bool setOfflineParameter(const char* identifier, double value);
    bool applyOfflineParameters(bool resetHistories = false);
    // DSP thread only; no allocations, locks, device I/O, dynamic module loading.
    EntropyResult process(const double* const* input13, double* const* output16,
                          std::uint32_t frames);
    GraphTelemetry telemetry() const noexcept;
    bool prepared() const noexcept { return prepared_; }

private:
    std::unique_ptr<entropy::standalone::EntropyCrossoverAudioProcessor> crossover_;
    entropy::output_matrix::OutputMatrixEngine matrix_;
    entropy::headroom::HeadroomEngine headroom_;
    PlanarBuffer crossoverBuffer_;
    PlanarBuffer matrixBuffer_;
    std::atomic<std::uint64_t> completedBlocks_{0};
    std::uint32_t maximumFrames_ = 0;
    bool prepared_ = false;
};
}
