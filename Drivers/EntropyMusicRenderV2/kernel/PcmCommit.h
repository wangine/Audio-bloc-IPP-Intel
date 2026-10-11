#pragma once
#include <portcls.h>
#include "WaveRtTimeline.h"
#include "OperationGate.h"
#include "../protocol/PcmBridgeV1.h"
bool ParseMusicFormat(PKSDATAFORMAT,entropy::music::PcmFormat&) noexcept;
class PcmCommit {
    uint8_t* pendingBytes_=nullptr;
    uint8_t* readyBytes_=nullptr;
    entropy::music::RingSlot* pendingSlots_=nullptr;
    entropy::music::RingSlot* readySlots_=nullptr;
    entropy::music::PcmRing pending_,ready_;
    KEVENT readIdle_;
    entropy::music::OperationGate operation_;
    uint64_t cachedFrames_=0,cachedPackets_=0,cachedQpc_=0;
public:
    uint64_t deliveredFrames=0;
    KSPIN_LOCK lock;
    EX_RUNDOWN_REF rundown;
    entropy::music::WaveRtTimeline timeline;
    PcmCommit() noexcept;
    ~PcmCommit();
    NTSTATUS Prepare(const entropy::music::PcmFormat&,uint64_t generation,uint64_t frequency,uint8_t* dma,uint32_t frames,uint32_t notifications);
    void Stop();
    NTSTATUS Read(void* output,uint32_t capacity,uint32_t& copied);
    bool TryEnter() noexcept {return operation_.tryEnter();}
    void EnterPassive();
    void Leave(uint64_t qpc=0);
    void PositionCache(uint64_t& frames,uint64_t& packets,uint64_t& qpc);
};
void InitializePcmOwners();
bool RegisterPcmOwner(PcmCommit*);
void UnregisterPcmOwner(PcmCommit*);
PcmCommit* AcquirePcmOwner();
void ReleasePcmOwner(PcmCommit*);
