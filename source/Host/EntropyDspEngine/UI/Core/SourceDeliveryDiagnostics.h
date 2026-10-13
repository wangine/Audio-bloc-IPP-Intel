#pragma once
#include <cstdint>

namespace entropy::host {
struct SourceDeliveryTelemetry {
    std::uint64_t deliveredBlocks=0, activeMissingBlocks=0, prerollBlocks=0;
    std::uint64_t idleBlocks=0, mutedBlocks=0, excludedBlocks=0;
    std::uint64_t generationChanges=0, generation=0, missingRun=0, maxMissingRun=0;
    std::uint32_t lastMissingFill=0;
};
// DSP-thread-owned observations only. Never gates or changes audio processing.
class SourceDeliveryDiagnostics {
public:
    SourceDeliveryTelemetry value{};
    void observe(bool selectedAndArmed, bool available, bool endpointActive,
                 bool muted, std::uint64_t generation, std::uint32_t fill) noexcept {
        if(generation!=value.generation) {
            if(value.generation) ++value.generationChanges;
            value.generation=generation; started_=false; value.missingRun=0;
        }
        if(!selectedAndArmed) { ++value.excludedBlocks; started_=false; value.missingRun=0; return; }
        if(muted) { ++value.mutedBlocks; value.missingRun=0; return; }
        if(available) { ++value.deliveredBlocks; started_=true; value.missingRun=0; return; }
        if(!endpointActive) { ++value.idleBlocks; value.missingRun=0; return; }
        if(!started_) { ++value.prerollBlocks; value.missingRun=0; return; }
        ++value.activeMissingBlocks;
        ++value.missingRun;
        if(value.missingRun>value.maxMissingRun) value.maxMissingRun=value.missingRun;
        value.lastMissingFill=fill;
    }
private:
    bool started_=false;
};
}
