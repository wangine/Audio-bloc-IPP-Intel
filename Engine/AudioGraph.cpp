#include "AudioGraph.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace entropy::engine {

bool AudioGraph::prepare(std::uint32_t maximumFrames) {
    prepared_ = false;
    if (!maximumFrames || maximumFrames > 4096) return false;
    // All object allocation, IPP dispatcher warmup and filter compilation are
    // strictly off the audio thread. Never call prepare while processing.
    try {
        auto candidate = std::make_unique<entropy::standalone::EntropyCrossoverAudioProcessor>();
        crossoverBuffer_.prepare(kOutputChannels, maximumFrames);
        matrixBuffer_.prepare(kOutputChannels, maximumFrames);
        candidate->prepareToPlay(kSampleRate, static_cast<int>(maximumFrames));

        if (!matrix_.prepare(kSampleRate, maximumFrames)) return false;
        EntropyOutputMatrixState routing{};
        routing.struct_size = sizeof(routing);
        routing.generation = 1;
        routing.active_source_mask = 0xffffu;
        routing.enabled_destination_mask = 0xffffu;
        for (std::uint32_t ch = 0; ch < kOutputChannels; ++ch)
            routing.source_destination_mask[ch] = 1u << ch;
        if (matrix_.setState(routing) != ENTROPY_OK) return false;

        if (!headroom_.prepare(kSampleRate, maximumFrames)) return false;
        EntropyHeadroomState safety{};
        safety.struct_size = sizeof(safety);
        safety.revision = 1;
        safety.group_count = 1;
        safety.active_channel_mask = 0xffffu;
        safety.transition_ms = 10.0;
        safety.groups[0].struct_size = sizeof(safety.groups[0]);
        safety.groups[0].bounded = 1;
        for (auto& group : safety.channel_group) group = 0;
        // OFFLINE FIXTURE ONLY. Explicit unity policy, NOT production headroom.
        // The real host must publish a validated ledger and gain policy before ARM.
        if (headroom_.setState(safety) != ENTROPY_OK) return false;
        if (headroom_.setMode(ENTROPY_HEADROOM_MODE_BIT_PERFECT) != ENTROPY_OK)
            return false;

        crossover_ = std::move(candidate);
        maximumFrames_ = maximumFrames;
        completedBlocks_.store(0, std::memory_order_relaxed);
        prepared_ = true;
        return true;
    } catch (...) {
        crossover_.reset();
        maximumFrames_ = 0;
        return false;
    }
}

EntropyResult AudioGraph::process(const double* const* input13, double* const* output16,
                                  std::uint32_t frames) {
    if (!prepared_ || !crossover_) return ENTROPY_ERROR_NOT_PREPARED;
    if (frames > maximumFrames_ || !input13 || !output16)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (frames == 0) return ENTROPY_OK;

    // Reject bad pointers and source data BEFORE advancing any DSP state.
    for (std::uint32_t ch = 0; ch < kInputChannels; ++ch) {
        if (!input13[ch]) return ENTROPY_ERROR_INVALID_ARGUMENT;
        for (std::uint32_t i = 0; i < frames; ++i)
            if (!std::isfinite(input13[ch][i])) return ENTROPY_ERROR_PROCESSING;
    }
    for (std::uint32_t ch = 0; ch < kOutputChannels; ++ch)
        if (!output16[ch]) return ENTROPY_ERROR_INVALID_ARGUMENT;

    for (std::uint32_t ch = 0; ch < kInputChannels; ++ch)
        std::copy_n(input13[ch], frames, crossoverBuffer_.channel(ch));
    for (std::uint32_t ch = kInputChannels; ch < kOutputChannels; ++ch)
        std::fill_n(crossoverBuffer_.channel(ch), frames, 0.0);

    // In-place 16-channel native Crossover input/output view; no JUCE, no heap.
    entropy::native::AudioBuffer<double> view(
        crossoverBuffer_.writePointers(), static_cast<int>(kOutputChannels),
        static_cast<int>(frames));
    entropy::native::MidiBuffer midi;
    crossover_->processBlock(view, midi);

    EntropyAudioBlockF64 block{};
    block.struct_size = sizeof(block);
    block.input_channels = kOutputChannels;
    block.output_channels = kOutputChannels;
    block.frames = frames;
    block.inputs = crossoverBuffer_.readPointers();
    block.outputs = matrixBuffer_.writePointers();
    auto result = matrix_.process(block);
    if (result != ENTROPY_OK) return result;

    block.inputs = matrixBuffer_.readPointers();
    block.outputs = output16;
    result = headroom_.process(block);
    if (result == ENTROPY_OK)
        completedBlocks_.fetch_add(1, std::memory_order_relaxed);
    return result;
}

GraphTelemetry AudioGraph::telemetry() const noexcept {
    GraphTelemetry result{};
    result.completedBlocks = completedBlocks_.load(std::memory_order_acquire);
    result.firLatencySamples = crossover_ ? static_cast<std::uint32_t>(
        std::max(0, crossover_->getReportedFirLatencySamples())) : 0;
    result.destinationMask = matrix_.telemetry().active_destination_mask;
    result.headroomFaults = headroom_.telemetry().fault_flags;
    return result;
}
}
