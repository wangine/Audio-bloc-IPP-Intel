#pragma once
#include "IPC/Json.h"
#include <WindowsDeviceInventory.h>
#include <DeviceIdentityResolver.h>
#include <array>
namespace entropy::control {
class DeviceService {
public:
 ipc::Value refresh();
 void restore(const ipc::Value&); // Candidate validation; no audio/device start.
 ipc::Value configure(const ipc::Value&);
 ipc::Value snapshot() const;
 ipc::Value bindingCommand(const ipc::Value&);
private:device::WindowsDeviceInventory inventory_;ipc::Value config_{ipc::Value::Array{}};std::array<EntropyDeviceBinding,2> bindings_{};std::array<bool,2> present_{};
};
}
