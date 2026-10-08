#include <entropy/dac/DacSinkCore.h>

#include <cstring>
#include <limits>

namespace entropy::dac
{
std::uint32_t bytesPerSample(std::uint32_t outputFormat) noexcept
{
    if (outputFormat == ENTROPY_PCM24_PACKED_LE) return 3;
    if (outputFormat == ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED) return 4;
    return 0;
}

bool validConfig(const EntropyDacSinkConfig& config) noexcept
{
    return config.struct_size >= sizeof(config) &&
        config.backend == ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE &&
        config.sample_rate >= 8000 && config.sample_rate <= 768000 &&
        config.channels > 0 && config.channels <= ENTROPY_PCM24_MAX_CHANNELS &&
        bytesPerSample(config.output_format) != 0 &&
        std::memchr(config.endpoint_id_utf8, 0,
                    ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES) != nullptr &&
        config.endpoint_id_utf8[0] != 0;
}

std::uint32_t blockAlignment(const EntropyDacSinkConfig& config) noexcept
{
    return config.channels * bytesPerSample(config.output_format);
}

std::uint64_t requiredBytes(const EntropyDacSinkConfig& config,
                            std::uint32_t frames) noexcept
{
    return static_cast<std::uint64_t>(blockAlignment(config)) * frames;
}

bool copyExact(std::uint8_t* destination, std::size_t destinationBytes,
               const EntropyDacSinkPcmBlock& block,
               const EntropyDacSinkConfig& config) noexcept
{
    if (destination == nullptr || block.struct_size < sizeof(block) ||
        block.data == nullptr || block.frames == 0)
        return false;
    const auto bytes = requiredBytes(config, block.frames);
    if (bytes > destinationBytes || bytes != block.byte_count ||
        bytes > (std::numeric_limits<std::size_t>::max)())
        return false;
    std::memcpy(destination, block.data, static_cast<std::size_t>(bytes));
    return true;
}
} // namespace entropy::dac
