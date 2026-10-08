#include <entropy/pcm24/Pcm24Converter.h>

#include <algorithm>
#include <cmath>
#include <bit>

namespace entropy::pcm24
{
namespace
{
std::uint64_t splitMix64(std::uint64_t& value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    auto mixed = value;
    mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ULL;
    mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebULL;
    return mixed ^ (mixed >> 31);
}

std::uint32_t nextPcg32(Pcg32Stream& stream) noexcept
{
    const auto previous = stream.state;
    stream.state = previous * 6364136223846793005ULL + stream.increment;
    const auto xorshifted = static_cast<std::uint32_t>(((previous >> 18u) ^ previous) >> 27u);
    const auto rotation = static_cast<std::uint32_t>(previous >> 59u);
    return (xorshifted >> rotation) | (xorshifted << ((0u - rotation) & 31u));
}

double unitInterval(Pcg32Stream& stream) noexcept
{
    // The half-LSB offset makes the finite generator symmetric around 0.5.
    return (static_cast<double>(nextPcg32(stream)) + 0.5) / 4294967296.0;
}

void initialiseStream(Pcg32Stream& stream, std::uint64_t& mixer) noexcept
{
    stream.state = 0;
    stream.increment = splitMix64(mixer) | 1ULL;
    nextPcg32(stream);
    stream.state += splitMix64(mixer);
    nextPcg32(stream);
}
}

void seedDither(DitherState& state, std::uint64_t seed,
                std::uint32_t channelIndex) noexcept
{
    auto mixer = seed ^ (0xd1b54a32d192ed03ULL *
        (static_cast<std::uint64_t>(channelIndex) + 1ULL));
    initialiseStream(state.uniformA, mixer);
    initialiseStream(state.uniformB, mixer);
}

double nextTpdfLsb(DitherState& state) noexcept
{
    return unitInterval(state.uniformA) - unitInterval(state.uniformB);
}

std::int32_t toQ31LeftAligned(double value) noexcept
{
    DitherState unused {};
    return toQ31LeftAligned(value, ENTROPY_PCM24_DITHER_NONE, unused);
}

std::int32_t toQ31LeftAligned(double value, EntropyPcm24DitherMode mode,
                              DitherState& state, bool* railClamped) noexcept
{
    // Quantize once at PCM24 resolution. std::round specifies nearest with
    // halfway cases away from zero, independent of the FP rounding mode.
    double u = value * 8388608.0;
    if (mode == ENTROPY_PCM24_DITHER_TPDF_FLAT)
        u += nextTpdfLsb(state);
    const double rounded = std::round(u);
    const double bounded = std::clamp(rounded, -8388608.0, 8388607.0);
    if (railClamped != nullptr) *railClamped = rounded != bounded;
    if (!std::isfinite(bounded)) return 0; // engine separately rejects NaN/Inf
    const auto q24 = static_cast<std::int32_t>(bounded);
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(q24) << 8);
}

void packLittleEndian(
    std::int32_t q31,
    std::uint8_t* destination) noexcept
{
    if (destination == nullptr)
        return;

    const std::uint32_t value = static_cast<std::uint32_t>(q31) >> 8;
    destination[0] = static_cast<std::uint8_t>(value & 0xff);
    destination[1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    destination[2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
}

void convertLittleEndian(
    std::span<const double> source,
    std::span<std::uint8_t> destination) noexcept
{
    const auto count = std::min(source.size(), destination.size() / 3);
    for (std::size_t index = 0; index < count; ++index)
        packLittleEndian(toQ31LeftAligned(source[index]),
                         destination.data() + index * 3);
}

void convertLittleEndian(std::span<const double> source,
                         std::span<std::uint8_t> destination,
                         EntropyPcm24DitherMode mode,
                         DitherState& state) noexcept
{
    const auto count = std::min(source.size(), destination.size() / 3);
    for (std::size_t index = 0; index < count; ++index)
        packLittleEndian(toQ31LeftAligned(source[index], mode, state),
                         destination.data() + index * 3);
}
} // namespace entropy::pcm24
