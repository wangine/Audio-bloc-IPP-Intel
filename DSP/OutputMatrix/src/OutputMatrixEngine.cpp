#include "OutputMatrixEngine.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace entropy::output_matrix
{
namespace
{
constexpr std::uint32_t allChannels = (1u << ENTROPY_OUTPUT_MATRIX_CHANNELS) - 1u;
static_assert(std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
}

bool OutputMatrixEngine::prepare(std::uint32_t sampleRate, std::uint32_t maximumFrames) noexcept
{
    if (sampleRate != 192000 || maximumFrames == 0 || maximumFrames > 65536) return false;
    maximumFrames_ = maximumFrames;
    prepared_ = true;
    reset();
    return true;
}

void OutputMatrixEngine::reset() noexcept
{
    acquireState(audioState_);
    appliedGeneration_.store(audioState_.generation, std::memory_order_relaxed);
    activeDestinations_.store(0, std::memory_order_relaxed);
    summingDestinations_.store(0, std::memory_order_relaxed);
    orphanedSources_.store(0, std::memory_order_relaxed);
    routeCount_.store(0, std::memory_order_relaxed);
    for (auto& peak : peaks_) peak.store(0.0, std::memory_order_relaxed);
    processedFrames_.store(0, std::memory_order_relaxed);
}

EntropyResult OutputMatrixEngine::setState(const EntropyOutputMatrixState& state) noexcept
{
    if (state.struct_size < sizeof(state) || (state.active_source_mask & ~allChannels) != 0 ||
        (state.enabled_destination_mask & ~allChannels) != 0) return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (const auto mask : state.source_destination_mask)
        if ((mask & ~allChannels) != 0) return ENTROPY_ERROR_INVALID_ARGUMENT;
    configSequence_.fetch_add(1, std::memory_order_acq_rel);
    generation_.store(state.generation, std::memory_order_relaxed);
    activeSources_.store(state.active_source_mask, std::memory_order_relaxed);
    enabledDestinations_.store(state.enabled_destination_mask, std::memory_order_relaxed);
    for (std::size_t source = 0; source < routes_.size(); ++source)
        routes_[source].store(state.source_destination_mask[source], std::memory_order_relaxed);
    configSequence_.fetch_add(1, std::memory_order_release);
    return ENTROPY_OK;
}

bool OutputMatrixEngine::acquireState(EntropyOutputMatrixState& result) const noexcept
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = configSequence_.load(std::memory_order_acquire);
        if ((before & 1u) != 0) continue;
        EntropyOutputMatrixState next {};
        next.struct_size = sizeof(next);
        next.generation = generation_.load(std::memory_order_relaxed);
        next.active_source_mask = activeSources_.load(std::memory_order_relaxed);
        next.enabled_destination_mask = enabledDestinations_.load(std::memory_order_relaxed);
        for (std::size_t source = 0; source < routes_.size(); ++source)
            next.source_destination_mask[source] = routes_[source].load(std::memory_order_relaxed);
        if (before == configSequence_.load(std::memory_order_acquire)) { result = next; return true; }
    }
    return false;
}

EntropyOutputMatrixState OutputMatrixEngine::state() const noexcept
{
    EntropyOutputMatrixState result {};
    result.struct_size = sizeof(result);
    acquireState(result);
    return result;
}

EntropyOutputMatrixTelemetry OutputMatrixEngine::telemetry() const noexcept
{
    EntropyOutputMatrixTelemetry result {};
    result.struct_size = sizeof(result);
    result.applied_generation = appliedGeneration_.load(std::memory_order_acquire);
    result.active_destination_mask = activeDestinations_.load(std::memory_order_acquire);
    result.summing_destination_mask = summingDestinations_.load(std::memory_order_acquire);
    result.orphaned_source_mask = orphanedSources_.load(std::memory_order_acquire);
    result.route_count = routeCount_.load(std::memory_order_acquire);
    result.host_engine_rate = 192000;
    for (std::size_t destination = 0; destination < peaks_.size(); ++destination)
        result.destination_peak[destination] = peaks_[destination].load(std::memory_order_acquire);
    result.processed_frames = processedFrames_.load(std::memory_order_acquire);
    return result;
}

EntropyResult OutputMatrixEngine::process(const EntropyAudioBlockF64& block) noexcept
{
    if (!prepared_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.frames > maximumFrames_ ||
        block.input_channels != ENTROPY_OUTPUT_MATRIX_CHANNELS ||
        block.output_channels != ENTROPY_OUTPUT_MATRIX_CHANNELS ||
        block.inputs == nullptr || block.outputs == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < ENTROPY_OUTPUT_MATRIX_CHANNELS; ++channel)
        if (block.inputs[channel] == nullptr || block.outputs[channel] == nullptr)
            return ENTROPY_ERROR_INVALID_ARGUMENT;
    EntropyOutputMatrixState next {};
    if (acquireState(next)) audioState_ = next;

    std::array<std::uint32_t, ENTROPY_OUTPUT_MATRIX_CHANNELS> contributors {};
    std::uint32_t orphaned = 0, routeCount = 0, activeDestinations = 0, summing = 0;
    for (std::uint32_t source = 0; source < ENTROPY_OUTPUT_MATRIX_CHANNELS; ++source)
    {
        if ((audioState_.active_source_mask & (1u << source)) == 0) continue;
        const auto effective = audioState_.source_destination_mask[source] &
                               audioState_.enabled_destination_mask;
        if (effective == 0) orphaned |= 1u << source;
        routeCount += std::popcount(effective);
        for (std::uint32_t destination = 0; destination < ENTROPY_OUTPUT_MATRIX_CHANNELS; ++destination)
            if ((effective & (1u << destination)) != 0) ++contributors[destination];
    }
    std::array<double, ENTROPY_OUTPUT_MATRIX_CHANNELS> peaks {};
    for (std::uint32_t destination = 0; destination < ENTROPY_OUTPUT_MATRIX_CHANNELS; ++destination)
    {
        if (contributors[destination] != 0) activeDestinations |= 1u << destination;
        if (contributors[destination] > 1) summing |= 1u << destination;
        auto* output = block.outputs[destination];
        if (contributors[destination] != 1) std::fill_n(output, block.frames, 0.0);
        for (std::uint32_t source = 0; source < ENTROPY_OUTPUT_MATRIX_CHANNELS; ++source)
        {
            if ((audioState_.active_source_mask & (1u << source)) == 0 ||
                (audioState_.source_destination_mask[source] & (1u << destination)) == 0 ||
                (audioState_.enabled_destination_mask & (1u << destination)) == 0) continue;
            const auto* input = block.inputs[source];
            if (contributors[destination] == 1)
                std::copy_n(input, block.frames, output);
            else
                for (std::uint32_t frame = 0; frame < block.frames; ++frame) output[frame] += input[frame];
        }
        for (std::uint32_t frame = 0; frame < block.frames; ++frame)
            peaks[destination] = (std::max)(peaks[destination], std::abs(output[frame]));
    }
    appliedGeneration_.store(audioState_.generation, std::memory_order_release);
    activeDestinations_.store(activeDestinations, std::memory_order_release);
    summingDestinations_.store(summing, std::memory_order_release);
    orphanedSources_.store(orphaned, std::memory_order_release);
    routeCount_.store(routeCount, std::memory_order_release);
    for (std::size_t destination = 0; destination < peaks_.size(); ++destination)
        peaks_[destination].store(peaks[destination], std::memory_order_release);
    processedFrames_.fetch_add(block.frames, std::memory_order_release);
    return ENTROPY_OK;
}
}
