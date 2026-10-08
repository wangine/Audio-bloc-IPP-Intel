#pragma once

#include <entropy/device_manager_abi.h>

#include <cstdint>
#include <vector>

namespace entropy::device
{
void normalizeDevice(EntropyDeviceInfo& device) noexcept;
EntropyDeviceBinding captureBinding(const EntropyDeviceInfo& device,
                                    const char* logicalDeviceId,
                                    std::uint64_t approvalRevision) noexcept;
EntropyDeviceBindingResolution resolveBinding(
    const EntropyDeviceBinding& binding,
    const std::vector<EntropyDeviceInfo>& devices) noexcept;
}
