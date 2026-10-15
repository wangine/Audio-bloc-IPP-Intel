#pragma once
#include <portcls.h>
#include "WaveRtTimeline.h"
#include "OperationGate.h"
#include "PacketAdmission.h"
#include "../model/CommitInbox.h"
#include "../model/DriverLedger.h"
#include "../protocol/PcmBridgeV1.h"
#include "../protocol/ClockBridgeV1.h"
bool ParseMusicFormat(PKSDATAFORMAT,entropy::music::PcmFormat&) noexcept;
class PcmCommit {
    uint8_t* pendingBytes_=nullptr;
    uint8_t* readyBytes_=nullptr;
    entropy::music::RingSlot* pendingSlots_=nullptr;
    entropy::music::RingSlot* readySlots_=nullptr;
    entropy::music::PcmRing pending_,ready_;
    entropy::music::CommitInbox inbox_;
    entropy::music::PacketAdmission admission_;
    uint8_t* inboxBytes_=nullptr;entropy::music::InboxSlot* inboxSlots_=nullptr;
    entropy::music::OperationGate writer_;
    EX_RUNDOWN_REF writers_;
    bool writersStopped_=false;
    bool ledgerEligible_=false;
    volatile LONG accepting_=0,terminal_=0;
    entropy::music::AtomicCount terminalFrames_,terminalPackets_;
    uint8_t* dma_=nullptr;uint32_t dmaFrames_=0,packetFrames_=0,notifications_=0,align_=0;
    entropy::music::Clock cachedClock_;
    KEVENT readIdle_,operationReleased_;
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
    bool SetRunning(bool running);
    NTSTATUS Submit(uint32_t number,bool eos,uint32_t eosBytes,uint64_t qpc);
    bool Flush();
    entropy::music::Totals CurrentTotals() const noexcept;
    void Archive();
    void MarkRegistered() noexcept{ledgerEligible_=true;}
    bool fault() const noexcept{return InterlockedCompareExchange(const_cast<volatile LONG*>(&terminal_),0,0)!=0;}
    NTSTATUS Read(void* output,uint32_t capacity,uint32_t& copied);
    // PASSIVE control entry; try the existing operation gate once. It does not
    // wait for audio, copy PCM, retire buffers or change packet admission.
    NTSTATUS ClockControl(const entropy::music::clockwire::Request&,uint64_t newLease,entropy::music::clockwire::Reply&);
    void ReleaseClockHandle(uint64_t lease) noexcept;
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
void GetPcmEvidence(uint64_t currentGeneration,const entropy::music::Totals& current,entropy::music::Totals& lifetime,entropy::music::Totals& ended);
