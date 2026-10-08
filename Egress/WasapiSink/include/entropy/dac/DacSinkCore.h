#pragma once

#include <entropy/dac_sink_abi.h>

#include <cstddef>
#include <cstdint>

namespace entropy::dac
{
bool validConfig(const EntropyDacSinkConfig& config) noexcept;
std::uint32_t bytesPerSample(std::uint32_t outputFormat) noexcept;
std::uint32_t blockAlignment(const EntropyDacSinkConfig& config) noexcept;
std::uint64_t requiredBytes(const EntropyDacSinkConfig& config,
                            std::uint32_t frames) noexcept;
bool copyExact(std::uint8_t* destination, std::size_t destinationBytes,
               const EntropyDacSinkPcmBlock& block,
               const EntropyDacSinkConfig& config) noexcept;
} // namespace entropy::dac
