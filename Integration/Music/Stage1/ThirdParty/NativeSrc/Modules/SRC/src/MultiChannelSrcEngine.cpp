#include "MultiChannelSrcEngine.h"

#include "../R8Brain/src/r8brain-free-src/CDSPResampler.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace entropy::src
{
namespace
{
double finiteOrSilence(double value) noexcept
{
    // A malformed sample is local to this channel/frame.  Feeding NaN/Inf into
    // a stateful SRC can poison its history indefinitely, so the documented
    // deterministic policy is substitution with FP64 zero.
    return std::isfinite(value) ? value : 0.0;
}

bool validConfig(const EntropySrcConfig& config, bool nominalEgress = false) noexcept
{
    return config.struct_size >= sizeof(config) &&
           config.engine == ENTROPY_SRC_R8BRAIN_REFERENCE &&
           config.source_rate >= 8000 && config.source_rate <= 768000 &&
           (nominalEgress
                ? (config.engine == ENTROPY_SRC_R8BRAIN_REFERENCE &&
                   config.source_rate == 192000 &&
                   (config.destination_rate == 192000 || config.destination_rate == 96000))
                : config.destination_rate == 192000) &&
           config.channels >= 1 && config.channels <= ENTROPY_SRC_MAX_CHANNELS &&
           config.maximum_input_frames >= 1 && config.maximum_input_frames <= 65536;
}
}

struct MultiChannelSrcEngine::Impl
{
    void clear()
    {
        r8brain.clear();
        channelInput.clear();
        r8Outputs.clear();
    }

    bool prepare(const EntropySrcConfig& config)
    {
        clear();
        const auto channels = static_cast<std::size_t>(config.channels);
        channelInput.resize(channels);
        for (auto& channel : channelInput) channel.resize(config.maximum_input_frames);
        r8Outputs.resize(channels);

        if (config.source_rate == config.destination_rate) return true;
        if (config.engine == ENTROPY_SRC_R8BRAIN_REFERENCE)
        {
            r8brain.reserve(channels);
            for (std::size_t channel = 0; channel < channels; ++channel)
                r8brain.push_back(std::make_unique<r8b::CDSPResampler>(
                    config.source_rate, config.destination_rate,
                    static_cast<int>(config.maximum_input_frames),
                    2.0, 217.0, r8b::fprLinearPhase));
            return true;
        }


        return false;
    }

    EntropyResult processR8brain(const EntropySrcConfig& config,
                                 EntropySrcBlockF64& block) noexcept
    {
        int produced = -1;
        for (std::uint32_t channel = 0; channel < config.channels; ++channel)
        {
            for (std::uint32_t frame = 0; frame < block.input_frames; ++frame)
                channelInput[channel][frame] = finiteOrSilence(block.inputs[channel][frame]);
            double* output = nullptr;
            const int count = r8brain[channel]->process(
                channelInput[channel].data(), static_cast<int>(block.input_frames), output);
            r8Outputs[channel] = output;
            if (produced < 0) produced = count;
            else if (produced != count) return ENTROPY_ERROR_PROCESSING;
        }
        if (produced < 0 || static_cast<std::uint32_t>(produced) > block.output_capacity)
            return ENTROPY_ERROR_PROCESSING;
        for (std::uint32_t channel = 0; channel < config.channels; ++channel)
            if (produced > 0)
            {
                if (r8Outputs[channel] == nullptr) return ENTROPY_ERROR_PROCESSING;
                std::memcpy(block.outputs[channel], r8Outputs[channel],
                            static_cast<std::size_t>(produced) * sizeof(double));
            }
        block.consumed_frames = block.input_frames;
        block.produced_frames = static_cast<std::uint32_t>(produced);
        return ENTROPY_OK;
    }

    std::vector<std::unique_ptr<r8b::CDSPResampler>> r8brain;
    std::vector<std::vector<double>> channelInput;
    std::vector<double*> r8Outputs;
};

MultiChannelSrcEngine::MultiChannelSrcEngine() : impl_(std::make_unique<Impl>()) {}
MultiChannelSrcEngine::~MultiChannelSrcEngine() = default;

std::uint32_t MultiChannelSrcEngine::requiredOutputCapacity(
    std::uint32_t sourceRate, std::uint32_t destinationRate,
    std::uint32_t maximumInputFrames) noexcept
{
    if (sourceRate == 0 || destinationRate == 0) return 0;
    const long double estimate = std::ceil(
        static_cast<long double>(maximumInputFrames) * destinationRate / sourceRate);
    const long double capacity = estimate + 8192.0L;
    return capacity > std::numeric_limits<std::uint32_t>::max()
        ? 0u : static_cast<std::uint32_t>(capacity);
}

bool MultiChannelSrcEngine::configure(const EntropySrcConfig& requested)
{
    if (!validConfig(requested)) return false;
    return configureValidated(requested);
}

bool MultiChannelSrcEngine::configureNominalEgress(const EntropySrcConfig& requested)
{
    if (!validConfig(requested, true)) return false;
    return configureValidated(requested);
}

bool MultiChannelSrcEngine::configureValidated(const EntropySrcConfig& requested)
{
    auto configured = requested;
    configured.struct_size = sizeof(configured);
    configured.required_output_capacity = requiredOutputCapacity(
        configured.source_rate, configured.destination_rate,
        configured.maximum_input_frames);
    if (configured.required_output_capacity == 0) return false;
    if (!impl_->prepare(configured)) return false;
    config_ = configured;
    return true;
}

void MultiChannelSrcEngine::reset()
{
    // Same-rate wire has no history. R8Brain provides an allocation-free
    // history clear; rebuilding filters here would allocate on egress push
    // when a track discontinuity is delivered.
    if (config_.source_rate == config_.destination_rate) return;
    if (config_.engine == ENTROPY_SRC_R8BRAIN_REFERENCE)
    {
        for (auto& channel : impl_->r8brain) channel->clear();
        return;
    }

}

EntropyResult MultiChannelSrcEngine::process(EntropySrcBlockF64& block) noexcept
{
    block.consumed_frames = 0;
    block.produced_frames = 0;
    if (config_.source_rate == 0) return ENTROPY_ERROR_NOT_PREPARED;
    const auto currentCapacity = config_.source_rate == config_.destination_rate
        ? block.input_frames
        : (std::min)(config_.required_output_capacity,
            requiredOutputCapacity(config_.source_rate, config_.destination_rate,
                                   block.input_frames));
    if (block.struct_size < sizeof(block) || block.channels != config_.channels ||
        block.input_frames > config_.maximum_input_frames ||
        block.output_capacity < currentCapacity ||
        block.inputs == nullptr || block.outputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        if (block.inputs[channel] == nullptr || block.outputs[channel] == nullptr)
            return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (config_.source_rate == config_.destination_rate)
    {
        if (block.output_capacity < block.input_frames) return ENTROPY_ERROR_INVALID_ARGUMENT;
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
            for (std::uint32_t frame = 0; frame < block.input_frames; ++frame)
                block.outputs[channel][frame] = finiteOrSilence(block.inputs[channel][frame]);
        block.consumed_frames = block.input_frames;
        block.produced_frames = block.input_frames;
        return ENTROPY_OK;
    }
    switch (config_.engine)
    {
        case ENTROPY_SRC_R8BRAIN_REFERENCE: return impl_->processR8brain(config_, block);
        default: return ENTROPY_ERROR_INVALID_STATE;
    }
}

std::uint32_t MultiChannelSrcEngine::latencyOutputFrames() const noexcept
{
    if (config_.source_rate == 0 || config_.source_rate == config_.destination_rate) return 0;
    return 0;
}
}
