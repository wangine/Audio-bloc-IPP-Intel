#pragma once

#include "EngineTelemetry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace entropy::host
{
struct EngineOutputLaneAssignment
{
    bool mapped = false;
    bool conflict = false;
    std::uint32_t domain = engineMaximumDacDomains;
    std::uint32_t physicalChannel = 0;
    EngineOutputLaneStatus status = EngineOutputLaneUnassigned;
};

// Physical assignment is configuration state, not ARM participation state.
// A configured DAC domain remains visible in Output Matrix while disabled for
// ARM. The enabled bit is intentionally ignored here.
inline std::array<EngineOutputLaneAssignment, engineMaximumDestinationLanes>
resolveOutputLaneAssignments(const EngineDacDomainControl* domains,
                             std::size_t domainCount) noexcept
{
    std::array<EngineOutputLaneAssignment, engineMaximumDestinationLanes> result{};
    if (domains == nullptr) return result;

    domainCount = (std::min)(domainCount,
        static_cast<std::size_t>(engineMaximumDacDomains));

    for (std::size_t domain = 0; domain < domainCount; ++domain)
    {
        const auto& configured = domains[domain];
        const auto channels = (std::min)(configured.channels,
            engineMaximumDestinationLanes);

        for (std::uint32_t physical = 0; physical < channels; ++physical)
        {
            const auto lane = configured.destinationLaneForChannel[physical];
            if (lane >= engineMaximumDestinationLanes) continue;

            auto& assignment = result[lane];
            if (assignment.mapped || assignment.conflict)
            {
                assignment = {};
                assignment.conflict = true;
                assignment.status = EngineOutputLaneUnassigned;
                continue;
            }

            assignment.mapped = true;
            assignment.domain = static_cast<std::uint32_t>(domain);
            assignment.physicalChannel = physical;
            assignment.status =
                configured.verifiedEndpointIdUtf8[0] != 0
                    ? EngineOutputLaneOnline
                    : configured.approvalRevision != 0
                        ? EngineOutputLaneOffline
                        : EngineOutputLaneUnassigned;
        }
    }
    return result;
}

template <std::size_t N>
inline auto resolveOutputLaneAssignments(
    const std::array<EngineDacDomainControl, N>& domains) noexcept
{
    return resolveOutputLaneAssignments(domains.data(), domains.size());
}

} // namespace entropy::host
