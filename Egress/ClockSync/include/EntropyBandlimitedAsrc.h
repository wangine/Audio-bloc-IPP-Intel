#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace entropy::asrc
{
// A shared, immutable polyphase fractional-delay bank for the DAC clock
// sinks. The engine and the DAC normally both run at 192 kHz, so this stage
// only tracks their small ppm difference. A 64-tap Kaiser-windowed sinc
// preserves the audible band while avoiding the high-frequency droop of the
// former two-point linear interpolator. Ninety-six taps also retain native
// 192 kHz programme energy through 90 kHz instead of treating it as noise.
class PolyphaseSinc
{
public:
    static constexpr int kTaps = 96;
    static constexpr int kPhases = 4096;
    static constexpr int kFirstOffset = -(kTaps / 2 - 1); // -31
    static constexpr int kLastOffset = kTaps / 2;          // +32

    struct Phase
    {
        int index = 0;
        double blend = 0.0;
    };

    struct Kernel
    {
        const double* first = nullptr;
        const double* second = nullptr;
        double blend = 0.0;
    };

    static void warmUp()
    {
        (void)table();
    }

    static Phase phaseFor(double fraction) noexcept
    {
        fraction -= std::floor(fraction);
        const double position = fraction * kPhases;
        int index = static_cast<int>(position);
        if (index < 0)
            index = 0;
        if (index >= kPhases)
            index = kPhases - 1;
        return {index, position - index};
    }

    static double coefficient(
        const Phase& phase,
        int tap) noexcept
    {
        const auto& coefficients = table();
        const double a = coefficients[static_cast<std::size_t>(phase.index)]
                                     [static_cast<std::size_t>(tap)];
        const double b = coefficients[static_cast<std::size_t>(phase.index + 1)]
                                     [static_cast<std::size_t>(tap)];
        return a + (b - a) * phase.blend;
    }

    static Kernel kernelFor(double fraction) noexcept
    {
        const auto phase = phaseFor(fraction);
        const auto& coefficients = table();
        return {coefficients[static_cast<std::size_t>(phase.index)].data(),
                coefficients[static_cast<std::size_t>(phase.index + 1)].data(),
                phase.blend};
    }

private:
    using Row = std::array<double, kTaps>;
    using Table = std::array<Row, kPhases + 1>;

    static double besselI0(double value) noexcept
    {
        double sum = 1.0;
        double term = 1.0;
        const double quarter = value * value * 0.25;
        for (int order = 1; order < 32; ++order)
        {
            term *= quarter /
                static_cast<double>(order * order);
            sum += term;
            if (term < sum * 1.0e-18)
                break;
        }
        return sum;
    }

    static double sinc(double value) noexcept
    {
        constexpr double pi =
            3.141592653589793238462643383279502884;
        if (std::abs(value) < 1.0e-14)
            return 1.0;
        const double angle = pi * value;
        return std::sin(angle) / angle;
    }

    static std::unique_ptr<Table> makeTable()
    {
        auto result = std::make_unique<Table>();
        constexpr double beta = 10.5; // approximately 110 dB sidelobes
        // Unity cutoff preserves the wideband drift response. Cardinal rows
        // are assigned exactly below; sin(pi*n) does not produce exact zeros.
        constexpr double cutoff = 1.0;
        const double denominator = besselI0(beta);
        constexpr double halfWidth = kTaps * 0.5;

        for (int phase = 0; phase <= kPhases; ++phase)
        {
            const double fraction =
                static_cast<double>(phase) / kPhases;
            double sum = 0.0;
            auto& row = (*result)[static_cast<std::size_t>(phase)];
            if (phase == 0 || phase == kPhases)
            {
                row.fill(0.0);
                row[static_cast<std::size_t>(-kFirstOffset + (phase == kPhases ? 1 : 0))] = 1.0;
                continue;
            }
            for (int tap = 0; tap < kTaps; ++tap)
            {
                const int offset = kFirstOffset + tap;
                const double distance =
                    static_cast<double>(offset) - fraction;
                const double normalized = distance / halfWidth;
                const double windowArgument =
                    beta * std::sqrt(std::max(
                        0.0, 1.0 - normalized * normalized));
                const double window =
                    besselI0(windowArgument) / denominator;
                const double value =
                    cutoff * sinc(cutoff * distance) * window;
                row[static_cast<std::size_t>(tap)] = value;
                sum += value;
            }
            // Exact DC gain for every fractional phase.
            if (sum != 0.0)
                for (auto& value : row)
                    value /= sum;
        }
        return result;
    }

    static const Table& table()
    {
        static const auto coefficients = makeTable();
        return *coefficients;
    }
};
} // namespace entropy::asrc
