#include "EndpointIdentity.h"
#include <cwchar>
#include <cstring>
namespace entropy::music::provider {
bool validIdentity(const DeviceIdentity& d) noexcept{
    bool nonzero=false;for(auto byte:d.containerId)nonzero|=byte!=0;
    return !_wcsicmp(d.hardwareId.c_str(),bridge::HardwareId) && !_wcsicmp(d.service.c_str(),bridge::ServiceName) &&
        !d.interfacePath.empty() && !d.windowsEndpointId.empty() && !std::memcmp(d.interfaceGuid.data(),bridge::InterfaceGuid,16) &&
        nonzero && d.containerId==d.endpointContainerId && d.flow==0 && d.state==1;
}
}
