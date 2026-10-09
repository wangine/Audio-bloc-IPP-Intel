#pragma once
#include <entropy/stage_bus_v1.h>
#include <cstdint>
#include <cstring>
namespace entropy::link {
inline constexpr uint64_t linkMagic=0x314B4E494C475453ULL;
enum class StageLinkAction:uint32_t {Query=1,Attach=2,Release=3};
enum class StageLinkStatus:uint32_t {Ok,Waiting,Busy,Refused,Fault};
struct StageLinkRequest {uint64_t magic=linkMagic;uint32_t version=1,bytes=sizeof(StageLinkRequest);StageLinkAction action=StageLinkAction::Query;uint32_t clientPid=0;uint8_t nonce[16]{},lease[16]{},expectedSession[16]{};uint64_t reserved[2]{};};
struct StageLinkResponse {uint64_t magic=linkMagic;uint32_t version=1,bytes=sizeof(StageLinkResponse);StageLinkStatus status=StageLinkStatus::Waiting;uint32_t sourcePid=0;uint8_t nonce[16]{},lease[16]{};StageBusSessionHeader session{};StageBusProgramSnapshot program{};uint64_t controlRevision=0,reserved[2]{};};
static_assert(sizeof(StageLinkRequest)==88 && sizeof(StageLinkResponse)==400);
inline bool zero(const uint8_t* bytes,size_t size) noexcept {uint8_t v=0;for(size_t i=0;i<size;i++)v|=bytes[i];return v==0;}
inline bool validRequest(const StageLinkRequest& r,uint32_t verifiedPid) noexcept {return r.magic==linkMagic && r.version==1 && r.bytes==sizeof(r) && r.clientPid && r.clientPid==verifiedPid && !zero(r.nonce,16) && !r.reserved[0] && !r.reserved[1] && r.action>=StageLinkAction::Query && r.action<=StageLinkAction::Release;}
StageLinkRequest makeRequest(StageLinkAction);
}
