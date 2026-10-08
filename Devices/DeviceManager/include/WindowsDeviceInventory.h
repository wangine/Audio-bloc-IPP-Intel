#pragma once

#include <entropy/device_manager_abi.h>

#include <vector>

namespace entropy::device
{
class WindowsDeviceInventory final
{
public:
    EntropyResult refresh();
    const std::vector<EntropyDeviceInfo>& devices() const noexcept { return devices_; }

private:
    std::vector<EntropyDeviceInfo> devices_;
};
}
