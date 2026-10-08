#pragma once

#include <entropy/device_manager_abi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace entropy::host::ui
{
inline bool deviceMatchesBinding(const EntropyDeviceInfo& device,
                                 const EntropyDeviceBinding& binding) noexcept
{
    const bool sameEndpoint = device.endpoint_id_utf8[0] != 0 &&
        binding.approved_endpoint_id_utf8[0] != 0 &&
        std::strcmp(device.endpoint_id_utf8,
                    binding.approved_endpoint_id_utf8) == 0;
    const bool sameFingerprint = device.composite_fingerprint[0] != 0 &&
        binding.composite_fingerprint[0] != 0 &&
        std::strcmp(device.composite_fingerprint,
                    binding.composite_fingerprint) == 0;
    return sameEndpoint || sameFingerprint;
}

inline std::uint32_t assignmentMaskForDevice(
    const EntropyDeviceInfo& device,
    const std::array<EntropyDeviceBinding, 2>& bindings,
    const std::array<bool, 2>& bindingPresent) noexcept
{
    std::uint32_t mask = 0;
    for (std::size_t slot = 0; slot < bindings.size(); ++slot)
        if (bindingPresent[slot] && deviceMatchesBinding(device, bindings[slot]))
            mask |= 1u << slot;
    return mask;
}

inline bool canAssignDeviceToSlot(
    const EntropyDeviceInfo& device,
    std::size_t targetSlot,
    const std::array<EntropyDeviceBinding, 2>& bindings,
    const std::array<bool, 2>& bindingPresent) noexcept
{
    for (std::size_t slot = 0; slot < bindings.size(); ++slot)
        if (slot != targetSlot && bindingPresent[slot] &&
            deviceMatchesBinding(device, bindings[slot]))
            return false;
    return targetSlot < bindings.size();
}

inline std::string_view assignmentLabel(std::uint32_t mask) noexcept
{
    switch (mask)
    {
        case 1u: return "ASSIGNED -> DAC_A";
        case 2u: return "ASSIGNED -> DAC_B";
        case 3u: return "CONFLICT: DAC_A + DAC_B";
        default: return "AVAILABLE";
    }
}

inline std::string shortPortToken(const char* interfacePath)
{
    if (interfacePath == nullptr) return {};
    const std::string path(interfacePath);
    const auto separator = path.find_last_of("\\/");
    return separator == std::string::npos ? path : path.substr(separator + 1u);
}
}
