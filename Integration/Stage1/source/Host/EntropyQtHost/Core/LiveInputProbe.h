#pragma once

#include "ModuleRegistry.h"
#include "AudioBlockReport.h"
#include <entropy/crossover_program_abi.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host
{
class GraphRuntime;
struct LiveInputProbeReport : AudioBlockReport
{
    std::array<std::vector<double>, 8> convertedAudio{};
    std::array<std::vector<double>, 16> finalAudio{};
    std::string detail;
};

class LiveInputProbe final
{
public:
    static LiveInputProbeReport runTidal(const ModuleRegistry& registry, bool muted = false);
    static LiveInputProbeReport runAux(const ModuleRegistry& registry, bool muted = false);
    static LiveInputProbeReport runCinema(const ModuleRegistry& registry);
    static LiveInputProbeReport runTidalCycle(GraphRuntime& runtime, bool& connected,
                                              std::uint32_t& configuredRate,
                                              std::uint32_t& configuredEngine,
                                              std::uint32_t& outputCapacity,
                                              std::uint32_t selectedEngine, bool muted);
    static LiveInputProbeReport runAuxCycle(GraphRuntime& runtime, bool& connected,
                                            std::uint32_t& configuredRate,
                                            std::uint32_t& configuredEngine,
                                            std::uint32_t& outputCapacity,
                                            std::uint32_t selectedEngine, bool muted);
    static LiveInputProbeReport runCinemaCycle(GraphRuntime& runtime, bool& connected,
                                               std::uint32_t& configuredRate,
                                               std::uint32_t& configuredEngine,
                                               std::uint32_t& outputCapacity,
                                               std::uint32_t selectedEngine);
    static LiveInputProbeReport runCombinedDownstream(
        GraphRuntime& runtime,
        const LiveInputProbeReport& tidal,
        const LiveInputProbeReport& aux,
        const LiveInputProbeReport& cinema,
        bool muteTidal, bool muteAux, bool muteCinema,
        bool musicProgramActive,
        bool analyzeMusicFinal = true,
        std::uint32_t forcedFrames = 0,
        const EntropyCrossoverProgramBlockApiV1* programApi = nullptr,
        const EntropyCrossoverProgramInputs* programInputs = nullptr);
};
} // namespace entropy::host
