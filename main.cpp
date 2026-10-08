#include "Engine/AudioGraph.h"
#include <iostream>
#include <string_view>
#if ENTROPY_STAGE05_CONTROL
#include "Engine/Control/CoreRuntime.h"
#endif
#if ENTROPY_STAGE04_EGRESS
#include "Engine/Egress/HardwareTest.h"
#endif

int main(int argc, char** argv) {
#if ENTROPY_STAGE04_EGRESS
    if (argc>1 && std::string_view(argv[1])=="--hardware-test")
        return entropy::egress_host::hardwareTest(argc,argv);
#endif
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
#if ENTROPY_STAGE05_CONTROL
    return entropy::control::runControlServer(argc,argv);
#endif
    std::cout
        << "EntropyAudioCore: native Stage3 integration CANDIDATE (offline only).\n"
        << "To run a non-hardware probe: EntropyAudioCore.exe --probe\n"
        << "To execute regression tests: ctest --test-dir build/msvc-release --output-on-failure\n"
        << "Physical output requires the separate explicit --hardware-test mode and offline PASS gate.\n"
        << "No DAC, Stage2 or IPC starts by default.\n";
    return 0;
}
