#pragma once
#include "../protocol/PcmBridgeV1.h"
#include <string>
#include <array>
#include <vector>
namespace entropy::music::provider {
struct DeviceIdentity {
    std::wstring hardwareId,service,interfacePath,windowsEndpointId;
    std::array<uint8_t,16> interfaceGuid{},containerId{},endpointContainerId{};
    uint32_t flow=0,state=0;
};
bool validIdentity(const DeviceIdentity&) noexcept;
std::vector<DeviceIdentity> resolveEndpoints();
std::string endpointKey(const DeviceIdentity&);
std::string utf8(const std::wstring&);
}
