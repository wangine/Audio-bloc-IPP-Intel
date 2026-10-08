#include "AudioGraph.h"
#include <algorithm>
namespace entropy::engine {
GraphTelemetry AudioGraph::telemetry() const noexcept {
    GraphTelemetry result{};
    result.completedBlocks = completedBlocks_.load(std::memory_order_acquire);
    result.firLatencySamples = crossover_ ? static_cast<std::uint32_t>(
        std::max(0, crossover_->getReportedFirLatencySamples())) : 0;
    result.destinationMask = matrix_.telemetry().active_destination_mask;
    result.headroomFaults = headroom_.telemetry().fault_flags;
    return result;
}
}
