#pragma once
#include <cstddef>
#include <entropy/stage_bus_v1.h>
namespace entropy::stage
{
enum class StageBusValidationResult
{
    Ok,
    Incompatible,
    Bounds,
    Identity,
    Order
};
StageBusSessionHeader makeSession(uint32_t layout, const uint8_t *uuid,
                                  uint64_t qpcFrequency) noexcept;
StageBusValidationResult validateSession(const StageBusSessionHeader &,
                                         size_t mappedBytes) noexcept;
StageBusValidationResult validateBlock(const StageBusSessionHeader &, const StageBusBlockHeader &,
                                       size_t slotBytes) noexcept;
StageBusValidationResult validateRequest(const StageBusSessionHeader &,
                                         const StageBusFrameRequest &) noexcept;
StageBusValidationResult validateSourceDemandRequest(const StageBusSessionHeader &,
                                                     const StageBusFrameRequest &) noexcept;
} // namespace entropy::stage
