#pragma once

#include "EngineTelemetry.h"

#include <cstring>

namespace entropy::host
{
inline bool duplicatePhysicalDac(const EngineDacDomainControl& first,
                                 const EngineDacDomainControl& second) noexcept
{
    if (first.enabled == 0 || second.enabled == 0) return false;

    const bool sameEndpoint = first.verifiedEndpointIdUtf8[0] != 0 &&
        second.verifiedEndpointIdUtf8[0] != 0 &&
        std::strcmp(first.verifiedEndpointIdUtf8,
                    second.verifiedEndpointIdUtf8) == 0;
    const bool sameFingerprint = first.verifiedFingerprint[0] != 0 &&
        second.verifiedFingerprint[0] != 0 &&
        std::strcmp(first.verifiedFingerprint,
                    second.verifiedFingerprint) == 0;
    return sameEndpoint || sameFingerprint;
}
}
