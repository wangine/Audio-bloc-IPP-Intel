#pragma once

#include <entropy/device_manager_abi.h>

#include <vector>
#include <string>

namespace entropy::device
{
class WindowsDeviceInventory final
{
public:
    EntropyResult refresh();
    EntropyResult refreshSelected(const std::vector<std::string>& endpoints);
    const std::vector<EntropyDeviceInfo>& devices() const noexcept { return devices_; }

private:
    EntropyResult refreshImpl(const std::vector<std::string>*);
    std::vector<EntropyDeviceInfo> devices_;
};
}
