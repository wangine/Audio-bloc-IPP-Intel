#include <entropy/pcm24/Pcm24Converter.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main()
{
    using namespace entropy::pcm24;
    require(isDitherModeImplemented(ENTROPY_PCM24_DITHER_NONE), "NONE unavailable");
    require(isDitherModeImplemented(ENTROPY_PCM24_DITHER_TPDF_FLAT), "TPDF unavailable");
    require(!isDitherModeImplemented(ENTROPY_PCM24_DITHER_RESERVED_FUTURE),
            "reserved future mode was accidentally enabled");

    // Legacy v2.1.7 path remains bit exact when dither is disabled.
    require(toQ31LeftAligned(-1.0) == std::numeric_limits<std::int32_t>::min(), "-FS law");
    require(toQ31LeftAligned(0.0) == 0, "zero law");
    require(toQ31LeftAligned(1.0) == 2147483392, "+FS law");
    constexpr std::array<double, 3> input {-1.0, 0.0, 1.0};
    std::array<std::uint8_t, 9> output {};
    convertLittleEndian(input, output);
    require(output == std::array<std::uint8_t, 9> {
        0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f},
        "legacy packed PCM24 changed");

    // Test mode: identical master seed and channel reproduce every value.
    DitherState first {}, replay {}, otherChannel {};
    constexpr std::uint64_t testSeed = 0x5450444654455354ULL;
    seedDither(first, testSeed, 3);
    seedDither(replay, testSeed, 3);
    seedDither(otherChannel, testSeed, 4);
    bool channelIsDecorrelated = false;
    for (int sample = 0; sample < 4096; ++sample)
    {
        const auto a = nextTpdfLsb(first);
        const auto b = nextTpdfLsb(replay);
        const auto c = nextTpdfLsb(otherChannel);
        require(a == b, "deterministic test sequence is not reproducible");
        channelIsDecorrelated = channelIsDecorrelated || a != c;
    }
    require(channelIsDecorrelated, "channel RNG streams are identical");

    // Distribution contract: U1-U2 in LSB units, support (-1,+1), zero mean,
    // variance 1/6. Near-endpoint coverage proves the 2 LSB p-p support is not
    // accidentally scaled to one LSB p-p.
    DitherState statistics {};
    seedDither(statistics, testSeed, 0);
    constexpr std::size_t count = 1'000'000;
    double minimum = 1.0, maximum = -1.0, sum = 0.0, sumSquares = 0.0;
    double previous = nextTpdfLsb(statistics), lagOneProduct = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto value = nextTpdfLsb(statistics);
        require(value > -1.0 && value < 1.0, "TPDF escaped +/-1 LSB support");
        minimum = (std::min)(minimum, value);
        maximum = (std::max)(maximum, value);
        sum += value;
        sumSquares += value * value;
        lagOneProduct += previous * value;
        previous = value;
    }
    const auto mean = sum / static_cast<double>(count);
    const auto variance = sumSquares / static_cast<double>(count) - mean * mean;
    require(minimum < -0.99 && maximum > 0.99, "TPDF support is not 2 LSB peak-to-peak");
    require(std::abs(mean) < 0.002, "TPDF mean is biased");
    require(std::abs(variance - 1.0 / 6.0) < 0.002, "TPDF variance is not triangular");
    require(std::abs(lagOneProduct / static_cast<double>(count) - mean * mean) < 0.001,
            "TPDF has excessive lag-one correlation instead of flat noise");

    DitherState left {}, right {};
    seedDither(left, testSeed, 0); seedDither(right, testSeed, 1);
    double leftSum = 0.0, rightSum = 0.0, crossProduct = 0.0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const auto l = nextTpdfLsb(left), r = nextTpdfLsb(right);
        leftSum += l; rightSum += r; crossProduct += l * r;
    }
    const auto crossCovariance = crossProduct / static_cast<double>(count) -
        (leftSum / static_cast<double>(count)) * (rightSum / static_cast<double>(count));
    require(std::abs(crossCovariance) < 0.001,
            "TPDF channel streams have excessive cross-correlation");

    DitherState noneState {}, tpdfState {};
    seedDither(noneState, testSeed, 0); seedDither(tpdfState, testSeed, 0);
    require(toQ31LeftAligned(0.25, ENTROPY_PCM24_DITHER_NONE, noneState) ==
            toQ31LeftAligned(0.25), "DITHER_NONE changed quantization");
    bool nonZeroSilence = false;
    for (int sample = 0; sample < 1024; ++sample)
        nonZeroSilence = nonZeroSilence ||
            toQ31LeftAligned(0.0, ENTROPY_PCM24_DITHER_TPDF_FLAT, tpdfState) != 0;
    require(nonZeroSilence, "TPDF was not applied before silence quantization");

    std::cout << "PASS PCM24 legacy NONE + flat TPDF +/-1 LSB, deterministic replay, "
                 "per-channel decorrelation and triangular statistics\n";
}
