#pragma once
#include "../model/ClockFollow.h"
#include <stddef.h>
namespace entropy::music::clockwire {
constexpr uint32_t Magic=0x314b4345,Version=0x00010000;
enum class Kind:uint32_t {Capabilities=1,Status=2,Acquire=3,Update=4,Release=5};
// Additive private METHOD_BUFFERED codes; require read AND write file access.
constexpr uint32_t ioctl(Kind kind){return (0x22u<<16)|(3u<<14)|((0x80fu+uint32_t(kind))<<2);}
struct Header {uint32_t magic,version,totalBytes,kind;};
struct Request {
    Header header;
    uint64_t streamGeneration,lease,masterEpoch,sequence;
    int32_t correctionPpb;
    uint32_t reserved;
};
struct Reply {
    Header header;
    uint64_t streamGeneration,lease,masterEpoch,sequence,qpcFrequency,qpcNow,expiresQpc,frames;
    int32_t requestedPpb,appliedPpb;
    uint32_t state,nominalRate,capabilities,maxAbsPpb,slewPpbPerSecond,leaseMilliseconds;
    uint64_t acceptedUpdates,rejectedUpdates;
};
static_assert(sizeof(Header)==16 && sizeof(Request)==56 && sizeof(Reply)==128);
static_assert(offsetof(Request,correctionPpb)==48 && offsetof(Reply,acceptedUpdates)==112);
constexpr uint32_t RateActuator=1u,HandleLease=2u,PreservesPcmBytes=4u,NominalOnExpiry=8u;
inline bool known(uint32_t code) noexcept {return code>=ioctl(Kind::Capabilities)&&code<=ioctl(Kind::Release)&&((code-ioctl(Kind::Capabilities))&3u)==0;}
inline bool validate(const Request& r,uint32_t code,uint32_t inputBytes,uint32_t outputBytes) noexcept {
    if(!known(code)||inputBytes!=sizeof(Request)||outputBytes!=sizeof(Reply)||r.header.magic!=Magic||r.header.version!=Version||
       r.header.totalBytes!=sizeof(Request)||r.header.kind<1||r.header.kind>5||code!=ioctl(Kind(r.header.kind))||r.reserved)return false;
    const auto k=Kind(r.header.kind);
    if(k==Kind::Capabilities||k==Kind::Status)return !r.streamGeneration&&!r.lease&&!r.masterEpoch&&!r.sequence&&!r.correctionPpb;
    if(!r.streamGeneration||!r.masterEpoch)return false;
    if(k==Kind::Acquire)return !r.lease&&!r.sequence&&!r.correctionPpb;
    if(!r.lease)return false;
    if(k==Kind::Release)return !r.sequence&&!r.correctionPpb;
    return r.sequence&&r.sequence!=UINT64_MAX&&r.correctionPpb>=-Clock::MaxCorrectionPpb&&r.correctionPpb<=Clock::MaxCorrectionPpb;
}
inline Reply reply(Kind k,const FollowSnapshot& s={}) noexcept {
    Reply r{};r.header={Magic,Version,sizeof(Reply),uint32_t(k)};
    r.streamGeneration=s.generation;r.lease=s.lease;r.masterEpoch=s.masterEpoch;r.sequence=s.sequence;
    r.qpcFrequency=s.frequency;r.qpcNow=s.now;r.expiresQpc=s.expires;r.frames=s.frames;
    r.requestedPpb=s.requestedPpb;r.appliedPpb=s.appliedPpb;r.state=uint32_t(s.state);r.nominalRate=s.rate;
    r.capabilities=RateActuator|HandleLease|PreservesPcmBytes|NominalOnExpiry;
    r.maxAbsPpb=Clock::MaxCorrectionPpb;r.slewPpbPerSecond=ClockFollow::SlewPpbPerSecond;r.leaseMilliseconds=ClockFollow::LeaseMilliseconds;
    r.acceptedUpdates=s.accepted;r.rejectedUpdates=s.rejected;return r;
}
}
