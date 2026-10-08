#include <entropy/pcm24/Pcm24ModuleEngine.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace entropy::pcm24
{
bool Pcm24ModuleEngine::configure(const EntropyPcm24Config& requested) noexcept
{
    if (requested.struct_size < sizeof(requested) ||
        !isDitherModeImplemented(static_cast<EntropyPcm24DitherMode>(requested.dither_mode)) ||
        requested.seed_mode > ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT ||
        requested.reserved_u32 > ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT ||
        requested.channels == 0 || requested.channels > ENTROPY_PCM24_MAX_CHANNELS ||
        requested.output_format > ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED ||
        requested.maximum_frames == 0 || requested.maximum_frames > 65536 ||
        (requested.seed_mode == ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT && requested.seed == 0))
    {
        faultFlags_.fetch_or(ENTROPY_PCM24_FAULT_INVALID_CONFIG, std::memory_order_relaxed);
        return false;
    }
    config_ = requested;
    config_.struct_size = sizeof(config_);
    configured_ = true;
    faultFlags_.store(ENTROPY_PCM24_FAULT_NONE, std::memory_order_relaxed);
    processedFrames_.store(0, std::memory_order_relaxed);
    overrangeCount_.store(0, std::memory_order_relaxed);
    nonfiniteCount_.store(0, std::memory_order_relaxed);
    railClampCount_.store(0, std::memory_order_relaxed);
    rawFaultBlockCount_.store(0, std::memory_order_relaxed);
    clipBlockCount_.store(0, std::memory_order_relaxed);
    seedAll();
    return true;
}

void Pcm24ModuleEngine::seedAll() noexcept
{
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        seedDither(dither_[channel], config_.seed, channel);
}

void Pcm24ModuleEngine::reset() noexcept
{
    if (!configured_) return;
    faultFlags_.store(ENTROPY_PCM24_FAULT_NONE, std::memory_order_relaxed);
    processedFrames_.store(0, std::memory_order_relaxed);
    overrangeCount_.store(0, std::memory_order_relaxed);
    nonfiniteCount_.store(0, std::memory_order_relaxed);
    railClampCount_.store(0, std::memory_order_relaxed);
    rawFaultBlockCount_.store(0, std::memory_order_relaxed);
    clipBlockCount_.store(0, std::memory_order_relaxed);
    if (config_.seed_mode == ENTROPY_PCM24_SEED_DETERMINISTIC_TEST)
        seedAll();
}

std::uint32_t Pcm24ModuleEngine::bytesPerSample() const noexcept
{
    return config_.output_format == ENTROPY_PCM24_PACKED_LE ? 3u : 4u;
}

void Pcm24ModuleEngine::zeroOutput(EntropyPcm24BlockF64& block,
                                   std::uint32_t bytes) noexcept
{
    if (block.output_interleaved != nullptr && block.output_capacity_bytes >= bytes)
        std::fill_n(block.output_interleaved, bytes, std::uint8_t{0});
    block.produced_bytes = block.output_capacity_bytes >= bytes ? bytes : 0;
}

EntropyResult Pcm24ModuleEngine::process(EntropyPcm24BlockF64& block) noexcept
{
    block.produced_bytes = 0;
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    if (block.struct_size < sizeof(block) || block.channels != config_.channels ||
        block.frames == 0 || block.frames > config_.maximum_frames ||
        block.inputs == nullptr || block.output_interleaved == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        if (block.inputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto bytes = static_cast<std::uint64_t>(block.frames) * config_.channels * bytesPerSample();
    if (bytes > block.output_capacity_bytes || bytes > (std::numeric_limits<std::uint32_t>::max)())
        return ENTROPY_ERROR_INVALID_ARGUMENT;

    if (faultFlags_.load(std::memory_order_relaxed) != ENTROPY_PCM24_FAULT_NONE)
    {
        zeroOutput(block, static_cast<std::uint32_t>(bytes));
        return ENTROPY_ERROR_PROCESSING;
    }
    if ((block.flags & ENTROPY_PCM24_FLAG_DISCONTINUITY) != 0 &&
        config_.seed_mode == ENTROPY_PCM24_SEED_DETERMINISTIC_TEST)
        seedAll();


    bool finite = true;
    std::uint64_t nonfinite = 0;
    std::uint64_t overrange = 0;
    for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        for (std::uint32_t frame = 0; frame < block.frames; ++frame)
        {
            const double value = block.inputs[channel][frame];
            if (!std::isfinite(value)) { finite = false; ++nonfinite; }
            else if (value < -1.0 || value > 1.0) ++overrange;
        }
    overrangeCount_.fetch_add(overrange, std::memory_order_relaxed);
    if (overrange != 0 && config_.reserved_u32 == ENTROPY_PCM24_OVERRANGE_FAIL_CLOSED)
    {
        rawFaultBlockCount_.fetch_add(1, std::memory_order_relaxed);
        faultFlags_.fetch_or(ENTROPY_PCM24_FAULT_RAW_OVERRANGE, std::memory_order_relaxed);
    }
    if (!finite)
    {
        nonfiniteCount_.fetch_add(nonfinite, std::memory_order_relaxed);
        faultFlags_.fetch_or(ENTROPY_PCM24_FAULT_NONFINITE_AUDIO, std::memory_order_relaxed);
        zeroOutput(block, static_cast<std::uint32_t>(bytes));
        return ENTROPY_ERROR_PROCESSING;
    }
    if (overrange != 0 && config_.reserved_u32 == ENTROPY_PCM24_OVERRANGE_FAIL_CLOSED)
    {
        zeroOutput(block, static_cast<std::uint32_t>(bytes));
        return ENTROPY_ERROR_PROCESSING;
    }

    if (overrange != 0) clipBlockCount_.fetch_add(1, std::memory_order_relaxed);
    const auto mode = static_cast<EntropyPcm24DitherMode>(config_.dither_mode);
    const auto stride = bytesPerSample();
    for (std::uint32_t frame = 0; frame < block.frames; ++frame)
        for (std::uint32_t channel = 0; channel < config_.channels; ++channel)
        {
            bool railClamped = false;
            const auto q31 = toQ31LeftAligned(block.inputs[channel][frame], mode,
                                               dither_[channel], &railClamped);
            if (railClamped && block.inputs[channel][frame] >= -1.0 && block.inputs[channel][frame] <= 1.0) railClampCount_.fetch_add(1, std::memory_order_relaxed);
            auto* destination = block.output_interleaved +
                (static_cast<std::size_t>(frame) * config_.channels + channel) * stride;
            if (stride == 3)
                packLittleEndian(q31, destination);
            else
            {
                destination[0] = static_cast<std::uint8_t>(q31 & 0xff);
                destination[1] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(q31) >> 8) & 0xff);
                destination[2] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(q31) >> 16) & 0xff);
                destination[3] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(q31) >> 24) & 0xff);
            }
        }
    block.produced_bytes = static_cast<std::uint32_t>(bytes);
    processedFrames_.fetch_add(block.frames, std::memory_order_relaxed);
    return ENTROPY_OK;
}

EntropyResult Pcm24ModuleEngine::clearFault() noexcept
{
    if (!configured_) return ENTROPY_ERROR_NOT_PREPARED;
    faultFlags_.store(ENTROPY_PCM24_FAULT_NONE, std::memory_order_relaxed);
    return ENTROPY_OK;
}

EntropyPcm24Telemetry Pcm24ModuleEngine::telemetry() const noexcept
{
    EntropyPcm24Telemetry value {};
    value.struct_size = sizeof(value);
    value.applied_revision = config_.revision;
    value.fault_flags = faultFlags_.load(std::memory_order_relaxed);
    value.dither_mode = config_.dither_mode;
    value.seed_mode = config_.seed_mode;
    value.output_format = config_.output_format;
    value.channels = config_.channels;
    value.bytes_per_sample = bytesPerSample();
    value.processed_frames = processedFrames_.load(std::memory_order_relaxed);
    value.overrange_sample_count = overrangeCount_.load(std::memory_order_relaxed);
    value.nonfinite_sample_count = nonfiniteCount_.load(std::memory_order_relaxed);
    value.reserved[0] = railClampCount_.load(std::memory_order_relaxed);
    value.reserved[1] = rawFaultBlockCount_.load(std::memory_order_relaxed);
    value.reserved[2] = clipBlockCount_.load(std::memory_order_relaxed);
    return value;
}
} // namespace entropy::pcm24


