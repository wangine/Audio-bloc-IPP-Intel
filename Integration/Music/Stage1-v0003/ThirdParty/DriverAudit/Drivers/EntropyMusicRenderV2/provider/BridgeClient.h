#pragma once
#include "EndpointIdentity.h"
#include "../../../Integration/Music/Stage1/ABI/endpoint_provider_v2.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
namespace entropy::music::provider {
enum class IoResult {Success,NoData,TooSmall,Cancelled,Failure};
class BridgeTransport {
public:
    virtual ~BridgeTransport()=default;
    virtual bool open(const DeviceIdentity&)=0;
    virtual IoResult read(void*,uint32_t,uint32_t&)=0;
    virtual bool info(bridge::Info&)=0;
    virtual void cancel()=0;
    virtual void close()=0;
};
class BridgeClient {
    friend struct BridgeClientTestProbe;
#ifdef ENTROPY_PROVIDER_TESTING
    void(*exitHook_)(void*)=nullptr;void* exitContext_=nullptr;
#endif
    BridgeTransport& transport_;
    std::vector<uint8_t> wire_;
    std::atomic<bool> cancelled_{false};
    std::mutex state_,reader_;
    std::condition_variable idle_;
    uint32_t active_=0;
    bool opened_=false,closing_=false;
    void(*observe_)(void*,const EntropyIngressPacketV2&,const void*)=nullptr;
    void* observation_=nullptr;
public:
    explicit BridgeClient(BridgeTransport& transport):transport_(transport){}
    ~BridgeClient(){close();}
    bool open(const DeviceIdentity&);
    void observation(void(*callback)(void*,const EntropyIngressPacketV2&,const void*),void* context){observe_=callback;observation_=context;}
    int32_t read(EntropyIngressPacketV2&,void*,uint32_t capacity);
    bool info(bridge::Info&);
    void cancel();
    void close();
};
}
