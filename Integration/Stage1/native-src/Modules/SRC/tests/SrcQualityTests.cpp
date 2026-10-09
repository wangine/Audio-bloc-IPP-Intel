#include "MultiChannelSrcEngine.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
constexpr double pi = 3.141592653589793238462643383279502884;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
double db(double value) { return 20.0 * std::log10((std::max)(value, 1.0e-20)); }

std::vector<double> render(std::uint32_t engine, std::uint32_t sourceRate,
                           double frequency, double seconds)
{
    constexpr std::uint32_t destinationRate = 192000;
    entropy::src::MultiChannelSrcEngine converter;
    EntropySrcConfig config {};
    config.struct_size = sizeof(config); config.engine = engine; config.source_rate = sourceRate;
    config.destination_rate = destinationRate; config.channels = 1; config.maximum_input_frames = 1024;
    require(converter.configure(config), "quality configure failed"); config = converter.config();
    std::vector<double> input(1024), output(config.required_output_capacity), rendered;
    const double ratio = static_cast<double>(destinationRate) / sourceRate;
    rendered.reserve(static_cast<std::size_t>(seconds * sourceRate * ratio) + 8192);
    const auto total = static_cast<std::uint64_t>(std::llround(seconds * sourceRate));
    const double* inputPointer = input.data(); double* outputPointer = output.data();
    for (std::uint64_t start = 0; start < total; start += 1024)
    {
        const auto frames = static_cast<std::uint32_t>((std::min<std::uint64_t>)(1024, total - start));
        for (std::uint32_t frame = 0; frame < frames; ++frame)
            input[frame] = 0.5 * std::sin(2.0 * pi * frequency * (start + frame) / sourceRate);
        EntropySrcBlockF64 block {};
        block.struct_size = sizeof(block); block.channels = 1; block.input_frames = frames;
        block.output_capacity = config.required_output_capacity; block.inputs = &inputPointer; block.outputs = &outputPointer;
        require(converter.process(block) == ENTROPY_OK, "quality process failed");
        rendered.insert(rendered.end(), output.begin(), output.begin() + block.produced_frames);
    }
    return rendered;
}

double toneAmplitude(const std::vector<double>& samples, std::size_t start, std::size_t count,
                     double frequency)
{
    constexpr double sampleRate = 192000.0;
    std::complex<double> accumulator {};
    const double increment = -2.0 * pi * frequency / sampleRate;
    auto oscillator = std::polar(1.0, increment * start);
    const auto rotation = std::polar(1.0, increment);
    for (std::size_t i = 0; i < count; ++i)
    {
        accumulator += samples[start + i] * oscillator;
        oscillator *= rotation;
    }
    return 2.0 * std::abs(accumulator) / count;
}

void passbandAndImage(std::uint32_t engine, std::uint32_t sourceRate)
{
    constexpr double frequency = 10000.0;
    const auto samples = render(engine, sourceRate, frequency, 2.0);
    constexpr std::size_t start = 96000, count = 192000;
    require(samples.size() >= start + count, "insufficient upsample output");
    const double wanted = toneAmplitude(samples, start, count, frequency);
    const double passbandDb = db(wanted / 0.5);
    require(std::abs(passbandDb) < 0.05, "passband outside 0.05 dB");
    if (sourceRate < 192000 && sourceRate > 20000)
    {
        const double imageFrequency = static_cast<double>(sourceRate) - frequency;
        if (imageFrequency > 0.0 && imageFrequency < 96000.0)
        {
            const double image = toneAmplitude(samples, start, count, imageFrequency);
            require(db(image / wanted) < -120.0, "upsample image rejection below 120 dB");
        }
    }
    std::cout << "PASS engine " << engine << ' ' << sourceRate
              << "->192000 passband " << passbandDb << " dB\n";
}

void downsampleRejection(std::uint32_t engine, std::uint32_t sourceRate)
{
    constexpr double wantedHz = 10000.0;
    constexpr double rejectedHz = 110000.0;
    constexpr double aliasHz = 82000.0;
    const auto wantedSamples = render(engine, sourceRate, wantedHz, 2.0);
    const auto rejectedSamples = render(engine, sourceRate, rejectedHz, 2.0);
    constexpr std::size_t start = 96000, count = 192000;
    require(wantedSamples.size() >= start + count && rejectedSamples.size() >= start + count,
            "insufficient downsample output");
    const double wanted = toneAmplitude(wantedSamples, start, count, wantedHz);
    const double passbandDb = db(wanted / 0.5);
    const double alias = toneAmplitude(rejectedSamples, start, count, aliasHz);
    require(std::abs(passbandDb) < 0.05, "downsample passband outside 0.05 dB");
    require(db(alias / 0.5) < -120.0, "downsample alias rejection below 120 dB");
    std::cout << "PASS engine " << engine << ' ' << sourceRate
              << "->192000 alias rejection\n";
}
}

int main()
{
    try
    {
        constexpr std::uint32_t lowerRates[] = {44100, 48000, 88200, 96000, 176400};
        constexpr std::uint32_t higherRates[] = {352800, 384000};
        for (std::uint32_t engine = 0; engine < 1; ++engine)
        {
            for (const auto rate : lowerRates) passbandAndImage(engine, rate);
            for (const auto rate : higherRates) downsampleRejection(engine, rate);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
