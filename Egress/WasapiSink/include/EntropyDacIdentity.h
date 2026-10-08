#pragma once

#include <EntropyOutputControlShared.h>

#include <algorithm>
#include <cwchar>

namespace entropy::physical
{
static_assert(outputcontrol::kDacSlots == 2,
              "DacSink contract requires exactly two physical domains");

inline int matchDacSlot(
    const outputcontrol::SharedOutputState& control,
    const control::OutputDestinationControl& destination,
    int destinationIndex, bool includeDisabled = false)
{
    for (int dac = 0; dac < outputcontrol::kDacSlots; ++dac)
    {
        const auto& settings = control.dac[dac];
        if ((includeDisabled || settings.enabled != FALSE) &&
            settings.endpointId[0] != L'\0' &&
            std::wcscmp(settings.endpointId, destination.endpointId) == 0)
        {
            return dac;
        }
    }

    for (int dac = 0; dac < outputcontrol::kDacSlots; ++dac)
    {
        const auto& settings = control.dac[dac];
        if ((!includeDisabled && settings.enabled == FALSE) || settings.endpointId[0] != L'\0')
            continue;
        if (settings.enabled == FALSE && settings.endpointName[0] == L'\0')
            continue;
        const int first = std::max(
            0, static_cast<int>(settings.firstDestination));
        const int count = std::max(
            1, static_cast<int>(settings.channelCount));
        if (destinationIndex >= first && destinationIndex < first + count)
            return dac;
    }

    for (int dac = 0; dac < outputcontrol::kDacSlots; ++dac)
    {
        const auto& settings = control.dac[dac];
        if ((includeDisabled || settings.enabled != FALSE) &&
            settings.endpointId[0] == L'\0' &&
            settings.endpointName[0] != L'\0' &&
            std::wcsstr(destination.displayName,
                        settings.endpointName) != nullptr)
        {
            return dac;
        }
    }
    return -1;
}

inline int requiredSinkChannels(
    int routedChannels,
    const outputcontrol::SharedOutputState& control,
    int matchedDac,
    int maximumChannels)
{
    if (matchedDac < 0 || matchedDac >= outputcontrol::kDacSlots)
        return std::clamp(routedChannels, 1, maximumChannels);
    return std::max(
        std::clamp(routedChannels, 1, maximumChannels),
        std::clamp(
            static_cast<int>(control.dac[matchedDac].channelCount),
            1,
            maximumChannels));
}
} // namespace entropy::physical
