#pragma once
#include "BridgeClient.h"
#include <windows.h>
namespace entropy::music::provider {
class NativeBridge:public BridgeTransport {
    HANDLE device_=INVALID_HANDLE_VALUE;
    HANDLE stop_=nullptr;
    HANDLE idleTimer_=nullptr;
public:
    ~NativeBridge() override {close();}
    bool open(const DeviceIdentity&) override;
    IoResult read(void*,uint32_t,uint32_t&) override;
    bool info(bridge::Info&) override;
    void cancel() override;
    void close() override;
};
}
