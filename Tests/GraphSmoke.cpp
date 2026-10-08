#include "Engine/AudioGraph.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using entropy::engine::AudioGraph;
    constexpr std::uint32_t frames = 256;
    AudioGraph graph;
    if (!graph.prepare(frames)) {
        std::cerr << "FAIL: graph.prepare\n";
        return 1;
    }
    std::array<std::vector<double>, AudioGraph::kInputChannels> input{};
    std::array<std::vector<double>, AudioGraph::kOutputChannels> output{};
    std::array<const double*, AudioGraph::kInputChannels> sources{};
    std::array<double*, AudioGraph::kOutputChannels> destinations{};
    for (std::size_t ch = 0; ch < input.size(); ++ch) {
        input[ch].assign(frames, 0.0);
        sources[ch] = input[ch].data();
    }
    for (std::size_t ch = 0; ch < output.size(); ++ch) {
        output[ch].assign(frames, 0.0);
        destinations[ch] = output[ch].data();
    }
    // Synthetic offline Music L/R signal; deterministic and below 0 dBFS.
    for (std::uint32_t block = 0; block < 64; ++block) {
        for (std::uint32_t f = 0; f < frames; ++f) {
            const double t = (static_cast<double>(block) * frames + f) / AudioGraph::kSampleRate;
            input[0][f] = 0.1 * std::sin(6.2831853071795864769 * 440.0 * t);
            input[1][f] = 0.1 * std::sin(6.2831853071795864769 * 660.0 * t);
        }
        if (graph.process(sources.data(), destinations.data(), frames) != ENTROPY_OK) {
            std::cerr << "FAIL: graph.process at block " << block << '\n';
            return 2;
        }
        for (const auto& way : output)
            for (double sample : way)
                if (!std::isfinite(sample)) {
                    std::cerr << "FAIL: nonfinite FP64 output\n";
                    return 3;
                }
    }
    const auto telemetry = graph.telemetry();
    if (telemetry.completedBlocks != 64 || telemetry.headroomFaults != 0) {
        std::cerr << "FAIL: telemetry blocks/faults\n";
        return 4;
    }
    std::cout << "PASS: native FP64 graph 13->16, 192kHz, 64 blocks x 256; no DAC\n";
    return 0;
}
