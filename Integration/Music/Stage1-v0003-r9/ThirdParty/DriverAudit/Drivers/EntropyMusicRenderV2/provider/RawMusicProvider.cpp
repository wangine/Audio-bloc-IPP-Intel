#include "NativeBridge.h"
#include "../protocol/ProviderStreamStateV1.h"
#include <cstring>
#include <new>
#ifdef ENTROPY_AUDIT_PROVIDER
#include "AuditObserver.h"
#endif
namespace entropy::music::provider {
namespace {
struct Session {NativeBridge transport;BridgeClient client{transport};
#ifdef ENTROPY_AUDIT_PROVIDER
    AuditObserver observer;
    Session(){client.observation([](void* context,const EntropyIngressPacketV2& p,const void* pcm){static_cast<AuditObserver*>(context)->observe(p,pcm);},&observer);}
#endif
    ~Session(){client.close();}
};
int32_t enumerate(uint32_t index,EntropyEndpointInfoV2* out){
    if(!out || out->structSize!=sizeof(*out))return -1;
    try{
        const auto devices=resolveEndpoints();if(index>=devices.size())return 0;const auto& d=devices[index];*out={};out->abiVersion=ENTROPY_PROVIDER_ABI_V2;out->structSize=sizeof(*out);
        out->roles=ENTROPY_ROLE_MUSIC;out->eligible=1;out->state=1;out->flow=0;out->accessModes=1u<<bridge::CoreTransportAccess;
        if(strcpy_s(out->endpointId,endpointKey(d).c_str()) || strcpy_s(out->windowsEndpointId,utf8(d.windowsEndpointId).c_str()))return -1;
        strcpy_s(out->name,"Entropy Music Render V2 - raw PCM");
        PcmFormat preferred{64,48000,2,16,16,4,192000,3,0xfffe,1,{},{}};std::memcpy(preferred.subFormat,PcmGuid,16);std::memcpy(&out->preferredFormat,&preferred,64);return 1;
    }catch(...){return -1;}
}
int32_t open(const char* id,uint32_t access,void** out){
    if(!id || !out || access!=bridge::CoreTransportAccess)return -1;*out=nullptr;
    try{for(const auto& device:resolveEndpoints()){if(endpointKey(device)!=id)continue;auto session=new Session;if(!session->client.open(device)){delete session;return -1;}*out=session;return 1;}}catch(...){return -1;}return -1;
}
int32_t read(void* opaque,EntropyIngressPacketV2* packet,void* payload,uint32_t capacity){if(!opaque || !packet)return -1;try{return static_cast<Session*>(opaque)->client.read(*packet,payload,capacity);}catch(...){return -1;}}
void cancel(void* opaque){if(opaque)static_cast<Session*>(opaque)->client.cancel();}
void close(void* opaque){delete static_cast<Session*>(opaque);}
}
}
extern "C" ENTROPY_PROVIDER_EXPORT int32_t entropy_endpoint_provider_v2(uint32_t abi,EntropyProviderApiV2* out){
    using namespace entropy::music::provider;
    if(!out || abi!=ENTROPY_PROVIDER_ABI_V2 || out->structSize!=sizeof(*out))return -1;
    *out={};out->abiVersion=abi;out->structSize=sizeof(*out);out->info.abiVersion=abi;out->info.structSize=sizeof(out->info);
    out->info.capabilities=ENTROPY_CAP_RAW_PCM|ENTROPY_CAP_STREAM_STATE_V1;out->info.roles=ENTROPY_ROLE_MUSIC;
    strcpy_s(out->info.providerId,entropy::music::bridge::ProviderId);strcpy_s(out->info.name,"Entropy Music Render V2 raw PCM Provider");
    out->enumerate=enumerate;out->open=open;out->read=read;out->cancel=cancel;out->close=close;return 1;
}
