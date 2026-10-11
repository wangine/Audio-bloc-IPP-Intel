#include "BridgeClient.h"
#include <cstring>
namespace entropy::music::provider {
bool BridgeClient::open(const DeviceIdentity& identity){
    std::lock_guard lock(state_);if(opened_ || closing_ || !validIdentity(identity))return false;
    wire_.resize(sizeof(bridge::Packet)+MaxPacketFrames*8);
    if(!transport_.open(identity))return false;
    bridge::Info info{};
    if(!transport_.info(info) || info.header.magic!=bridge::Magic || info.header.version!=bridge::Version ||
        info.header.kind!=2 || info.header.totalBytes!=sizeof(info) || std::memcmp(info.interfaceGuid,bridge::InterfaceGuid,16)) {
        transport_.close();return false;
    }
    for(auto v:info.reserved)if(v){transport_.close();return false;}
    cancelled_=false;opened_=true;return true;
}
int32_t BridgeClient::read(EntropyIngressPacketV2& out,void* payload,uint32_t capacity){
    if(!payload || !capacity)return -1;
    std::unique_lock reader(reader_,std::try_to_lock);if(!reader.owns_lock())return -1;
    {std::lock_guard lock(state_);if(!opened_ || closing_)return -1;++active_;}
    struct ReadLease {BridgeClient& client;~ReadLease(){std::lock_guard lock(client.state_);--client.active_;client.idle_.notify_all();}} lease{*this};
    if(cancelled_)return 0;
    const auto pcmCapacity=capacity<MaxPacketFrames*8?capacity:MaxPacketFrames*8;
    uint32_t copied=0;
    const auto result=transport_.read(wire_.data(),static_cast<uint32_t>(sizeof(bridge::Packet))+pcmCapacity,copied);
    if(cancelled_ || result==IoResult::Cancelled || result==IoResult::NoData)return 0;
    if(result!=IoResult::Success || copied<sizeof(bridge::Packet) || copied>wire_.size())return -1;
    bridge::Packet packet{};std::memcpy(&packet,wire_.data(),sizeof(packet));
    if(bridge::validate(packet,copied,static_cast<uint32_t>(sizeof(packet))+pcmCapacity)!=bridge::DecodeResult::Valid)return -1;
    EntropyIngressPacketV2 p{};p.abiVersion=ENTROPY_PROVIDER_ABI_V2;p.structSize=sizeof(p);
    std::memcpy(&p.format,&packet.format,sizeof(p.format));const auto& d=packet.descriptor;
    p.generation=d.generation;p.packetSequence=d.sequence;p.firstSourceFrame=d.firstSourceFrame;p.qpcTimestamp=d.qpcTimestamp;
    p.qpcFrequency=d.qpcFrequency;p.lostFrames=d.lostFrames;p.frames=d.frames;p.flags=d.flags&(RawIdentity|Discontinuity);
    p.mode=d.mode;p.payloadBytes=packet.payloadBytes;p.originalContainerBits=p.format.containerBits;p.originalValidBits=p.format.validBits;
    std::memcpy(payload,wire_.data()+sizeof(packet),packet.payloadBytes);out=p;
    if(observe_)observe_(observation_,out,payload);return 1;
}
bool BridgeClient::info(bridge::Info& out){std::lock_guard lock(state_);return opened_ && !closing_ && transport_.info(out);}
void BridgeClient::cancel(){std::lock_guard lock(state_);if(opened_){cancelled_=true;transport_.cancel();}}
void BridgeClient::close(){
    std::unique_lock lock(state_);if(!opened_ || closing_)return;closing_=true;cancelled_=true;transport_.cancel();
    idle_.wait(lock,[&](){return !active_;});transport_.close();opened_=false;closing_=false;
}
}
