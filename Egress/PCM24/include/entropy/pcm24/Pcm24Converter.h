#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <entropy/pcm24_abi.h>

namespace entropy::pcm24
{
inline constexpr double pcm24Lsb = 1.0 / 8388608.0;

struct Pcg32Stream
{
    std::uint64_t state = 0;
    std::uint64_t increment = 1;
};

struct DitherState
{
    Pcg32Stream uniformA;
    Pcg32Stream uniformB;
};

// The host supplies productionEntropy outside the realtime thread. channelIndex
// is mixed into two independent PCG streams, so channels never share state.
void seedDither(DitherState& state, std::uint64_t seed,
                std::uint32_t channelIndex) noexcept;

// TPDF = U1 - U2, with U1/U2 in [0,1). Returned units have support (-1,+1),
// therefore exactly 2 LSB peak-to-peak as a distribution before quantization.
double nextTpdfLsb(DitherState& state) noexcept;

constexpr bool isDitherModeImplemented(EntropyPcm24DitherMode mode) noexcept
{
    return mode == ENTROPY_PCM24_DITHER_NONE ||
           mode == ENTROPY_PCM24_DITHER_TPDF_FLAT;
}

// Signed PCM24, nearest integer with ties away from zero, left-aligned in Q31.
std::int32_t toQ31LeftAligned(double value) noexcept;

std::int32_t toQ31LeftAligned(
    double value,
    EntropyPcm24DitherMode mode,
    DitherState& state, bool* railClamped = nullptr) noexcept;

// Packs one left-aligned Q31 value as signed little-endian packed PCM24.
void packLittleEndian(std::int32_t q31, std::uint8_t* destination) noexcept;

void convertLittleEndian(
    std::span<const double> source,
    std::span<std::uint8_t> destination) noexcept;

void convertLittleEndian(
    std::span<const double> source,
    std::span<std::uint8_t> destination,
    EntropyPcm24DitherMode mode,
    DitherState& state) noexcept;
} // namespace entropy::pcm24
