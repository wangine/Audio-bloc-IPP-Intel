#include "Engine/AudioGraph.h"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    constexpr std::string_view probe = "--probe";
    if (argc == 2 && std::string_view(argv[1]) == probe) {
        entropy::engine::AudioGraph graph;
        if (!graph.prepare(256)) {
            std::cerr << "AudioGraph native/IPP prepare FAIL\n";
            return 1;
        }
        std::cout << "AudioGraph native FP64/192 prepare PASS; "
                  << "13 inputs -> 16 outputs; no hardware or IPC started\n";
        return 0;
    }
    std::cout
        << "EntropyAudioCore: native Stage3 integration CANDIDATE (offline only).\n"
        << "To run a non-hardware probe: EntropyAudioCore.exe --probe\n"
        << "To execute regression tests: ctest --test-dir build/msvc-release --output-on-failure\n"
        << "Physical DAC start, Stage2 transport and IPC are NOT IMPLEMENTED in this baseline.\n";
    return 0;
}
