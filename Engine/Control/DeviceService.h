#pragma once
#include "IPC/Json.h"
#include <WindowsDeviceInventory.h>
#include <DeviceIdentityResolver.h>
#include <array>
#include "DeviceChangeCache.h"
namespace entropy::control {
class DeviceService {
public:
 ipc::Value refresh();
 ipc::Value revalidateSelected();
 // Return a transient per-endpoint preflight without changing saved bindings.
 ipc::Value independentPlaybackConfiguration();
 void closeNotifications() noexcept {changes_.close();}
 void restore(const ipc::Value&);
 ipc::Value configure(const ipc::Value&);
 ipc::Value snapshot() const;
 ipc::Value bindingCommand(const ipc::Value&);
private:device::WindowsDeviceInventory inventory_;ipc::Value config_{ipc::Value::Array{}};std::array<EntropyDeviceBinding,2> bindings_{};std::array<bool,2> present_{};
 DeviceChangeCache changes_;uint64_t evidenceGeneration_=0;std::vector<std::string> validatedEndpoints_;
};
}
