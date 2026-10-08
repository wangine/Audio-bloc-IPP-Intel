#pragma once
#include <cstdint>

namespace entropy::host {
struct SourceDeliveryTelemetry {
    std::uint64_t deliveredBlocks=0, activeMissingBlocks=0, prerollBlocks=0;
    std::uint64_t idleBlocks=0, mutedBlocks=0, excludedBlocks=0;
    std::uint64_t generationChanges=0, generation=0, missingRun=0, maxMissingRun=0;
    std::uint32_t lastMissingFill=0;
};
}