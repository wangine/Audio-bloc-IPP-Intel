#include "PcmCommit.h"
using namespace entropy::music;
namespace {constexpr ULONG Tag='2RPM';KSPIN_LOCK ownerLock;PcmCommit* owner=nullptr;bool ownerDraining=false;DriverLedger evidence;}
bool ParseMusicFormat(PKSDATAFORMAT data,PcmFormat& f) noexcept {
    if(!data || data->FormatSize<sizeof(KSDATAFORMAT_WAVEFORMATEX) ||
        !IsEqualGUID(data->MajorFormat,KSDATAFORMAT_TYPE_AUDIO) || !IsEqualGUID(data->SubFormat,KSDATAFORMAT_SUBTYPE_PCM) ||
        !IsEqualGUID(data->Specifier,KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))return false;
    const auto w=reinterpret_cast<const WAVEFORMATEX*>(data+1);
    f={64,w->nSamplesPerSec,w->nChannels,w->wBitsPerSample,w->wBitsPerSample,w->nBlockAlign,w->nAvgBytesPerSec,3,0xfffe,1,{},{}};
    RtlCopyMemory(f.subFormat,PcmGuid,16);
    if(w->wFormatTag==WAVE_FORMAT_EXTENSIBLE) {
        if(w->cbSize!=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX) || data->FormatSize<sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE))return false;
        const auto ext=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(w);
        f.validBits=ext->Samples.wValidBitsPerSample;f.channelMask=ext->dwChannelMask;RtlCopyMemory(f.subFormat,&ext->SubFormat,16);
    } else if(w->wFormatTag!=WAVE_FORMAT_PCM || w->wBitsPerSample!=16 || w->cbSize)return false;
    // PCM16 WAVEFORMATEX metadata is canonicalized to extensible/PCM/mask3.
    // Every PCM payload byte remains unchanged, including the unused low byte
    // in the PCM24/32 container.
    return validateFormat(f);
}
PcmCommit::PcmCommit() noexcept {KeInitializeSpinLock(&lock);ExInitializeRundownProtection(&rundown);ExInitializeRundownProtection(&writers_);KeInitializeEvent(&readIdle_,NotificationEvent,TRUE);}
PcmCommit::~PcmCommit() {
    Stop();
    if(pendingBytes_)ExFreePoolWithTag(pendingBytes_,Tag);
    if(readyBytes_)ExFreePoolWithTag(readyBytes_,Tag);
    if(pendingSlots_)ExFreePoolWithTag(pendingSlots_,Tag);
    if(readySlots_)ExFreePoolWithTag(readySlots_,Tag);
    if(inboxBytes_)ExFreePoolWithTag(inboxBytes_,Tag);
    if(inboxSlots_)ExFreePoolWithTag(inboxSlots_,Tag);
}
NTSTATUS PcmCommit::Prepare(const PcmFormat& f,uint64_t generation,uint64_t frequency,uint8_t* dma,uint32_t frames,uint32_t notifications) {
    PAGED_CODE();
    EnterPassive();
    struct Lease {PcmCommit& pcm;~Lease(){pcm.Leave(KeQueryPerformanceCounter(nullptr).QuadPart);}} lease{*this};
    if(timeline.prepared())return STATUS_INVALID_DEVICE_STATE;
    if(!pendingBytes_) {
        pendingBytes_=static_cast<uint8_t*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,static_cast<SIZE_T>(frames)*f.blockAlign,Tag));
        readyBytes_=static_cast<uint8_t*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,static_cast<SIZE_T>(RingFrames)*f.blockAlign,Tag));
        pendingSlots_=static_cast<RingSlot*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,64*sizeof(RingSlot),Tag));
        readySlots_=static_cast<RingSlot*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,RingDescriptors*sizeof(RingSlot),Tag));
        inboxBytes_=static_cast<uint8_t*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,static_cast<SIZE_T>(frames)*f.blockAlign,Tag));
        inboxSlots_=static_cast<InboxSlot*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,64*sizeof(InboxSlot),Tag));
        if(!pendingBytes_ || !readyBytes_ || !pendingSlots_ || !readySlots_ || !inboxBytes_ || !inboxSlots_)return STATUS_INSUFFICIENT_RESOURCES;
        if(!pending_.init(pendingBytes_,frames,f.blockAlign,pendingSlots_,64) || !ready_.init(readyBytes_,RingFrames,f.blockAlign,readySlots_,RingDescriptors))return STATUS_INVALID_PARAMETER;
    }
    if(!timeline.prepare(f,generation,frequency,dma,frames,notifications,pending_,ready_))return STATUS_INVALID_PARAMETER;
    if(!inbox_.init(inboxBytes_,frames,f.blockAlign,inboxSlots_,64))return STATUS_INVALID_PARAMETER;
    dma_=dma;dmaFrames_=frames;notifications_=notifications;packetFrames_=frames/notifications;align_=f.blockAlign;
    admission_.reset(generation,frequency,packetFrames_,notifications,align_);
    terminalFrames_.set(0);terminalPackets_.set(0);InterlockedExchange(&terminal_,0);
    if(writersStopped_){ExReInitializeRundownProtection(&writers_);writersStopped_=false;}
    InterlockedExchange(&accepting_,1);deliveredFrames=0;return STATUS_SUCCESS;
}
NTSTATUS PcmCommit::Submit(uint32_t number,bool eos,uint32_t eosBytes,uint64_t qpc){
    if(!InterlockedCompareExchange(&accepting_,0,0) || !ExAcquireRundownProtection(&writers_))return STATUS_INVALID_DEVICE_STATE;
    struct WriterLease {EX_RUNDOWN_REF& r;~WriterLease(){ExReleaseRundownProtection(&r);}} lease{writers_};
    if(!InterlockedCompareExchange(&accepting_,0,0) || fault())return STATUS_DATA_ERROR;
    // A second concurrent OS producer is a terminal protocol violation. It
    // cannot be retried silently or guessed into chronological PCM evidence.
    if(!writer_.tryEnter()){InterlockedExchange(&terminal_,1);return STATUS_DATA_ERROR;}
    struct ProducerLease {OperationGate& gate;~ProducerLease(){gate.leave();}} producer{writer_};
    Clock clock;KIRQL old;KeAcquireSpinLock(&lock,&old);clock=cachedClock_;KeReleaseSpinLock(&lock,old);
    uint64_t source=0;PacketDescriptor d{};
    if(!clock.position(qpc,source)){InterlockedExchange(&terminal_,1);return STATUS_DATA_ERROR;}
    const auto result=admission_.acceptWindowsPacket(number,eos,eosBytes,source,qpc,d,clock.running());
    if(result==CommitResult::Duplicate)return STATUS_DATA_LATE_ERROR;
    if(result==CommitResult::Overrun)return STATUS_DATA_OVERRUN;
    if(result!=CommitResult::Accepted && result!=CommitResult::Late)return STATUS_INVALID_PARAMETER;
    const auto offset=static_cast<uint32_t>((d.firstSourceFrame/packetFrames_)%notifications_)*packetFrames_*align_;
    const auto stored=inbox_.push(d,d.frames?dma_+offset:nullptr,result==CommitResult::Late);
    if(stored==InboxResult::Full || stored==InboxResult::Invalid){
        terminalFrames_.set(terminalFrames_.get()+d.frames);terminalPackets_.set(terminalPackets_.get()+1);
        InterlockedExchange(&terminal_,1);return STATUS_DATA_ERROR;
    }
    return result==CommitResult::Late?STATUS_DATA_LATE_ERROR:STATUS_SUCCESS;
}
bool PcmCommit::Flush(){
    InboxView v{};
    // Bounded by 64 records even if the independent producer keeps writing.
    for(uint32_t n=0;n<64 && inbox_.peek(v);++n){
        const auto result=timeline.captureSnapshot(v.pcm,v.dropped);
        if(result!=CommitResult::Accepted && result!=CommitResult::Overrun){InterlockedExchange(&terminal_,1);return false;}
        if(!inbox_.consume(v.pcm.packet.sequence)){InterlockedExchange(&terminal_,1);return false;}
    }
    return !fault();
}
Totals PcmCommit::CurrentTotals() const noexcept{
    auto t=timeline.totals();t.committedFrames+=inbox_.queuedFrames()+terminalFrames_.get();t.committedPackets+=inbox_.queuedRecords()+terminalPackets_.get();
    t.lostFrames+=inbox_.queuedLostFrames()+terminalFrames_.get();return t;
}
bool PcmCommit::SetRunning(bool running){
    PAGED_CODE();InterlockedExchange(&accepting_,0);
    if(!writersStopped_){ExWaitForRundownProtectionRelease(&writers_);writersStopped_=true;}
    EnterPassive();const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;
    const bool flushed=Flush();bool ok=false;
    if(timeline.prepared()){
        if(running)ok=flushed && timeline.start(qpc);
        else ok=!timeline.running() || timeline.pause(qpc);
    }
    Leave(qpc);
    if(timeline.prepared()){
        ExReInitializeRundownProtection(&writers_);writersStopped_=false;
        InterlockedExchange(&accepting_,1);
    }
    return ok;
}
void PcmCommit::Archive(){
    if(timeline.prepared() || !ledgerEligible_)return;
    auto t=CurrentTotals();const auto queued=inbox_.queuedFrames(),lost=inbox_.queuedLostFrames();t.cancelledFrames+=queued-lost;
    KIRQL old;KeAcquireSpinLock(&ownerLock,&old);evidence.finish(timeline.generation(),t);KeReleaseSpinLock(&ownerLock,old);
}
void PcmCommit::Stop() {
    PAGED_CODE();
    InterlockedExchange(&accepting_,0);
    if(!writersStopped_){ExWaitForRundownProtectionRelease(&writers_);writersStopped_=true;}
    EnterPassive();const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;
    Flush();const bool complete=timeline.requestStop(qpc);if(complete)Archive();Leave(qpc);
    if(!complete)KeWaitForSingleObject(&readIdle_,Executive,KernelMode,FALSE,nullptr);
}
NTSTATUS PcmCommit::Read(void* output,uint32_t capacity,uint32_t& copied) {
    PAGED_CODE();copied=0;PacketView v{};
    if(!TryEnter())return STATUS_DEVICE_NOT_READY;
    const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;Flush();if(timeline.prepared() && timeline.running())timeline.advance(qpc,false);
    if(!timeline.beginRead(v)){Leave();return STATUS_DEVICE_NOT_READY;}
    KeClearEvent(&readIdle_);
    const uint32_t total=static_cast<uint32_t>(sizeof(bridge::Packet))+v.firstBytes+v.secondBytes;
    const auto format=timeline.format();
    Leave();
    if(capacity>=total) {
        bridge::Packet packet{{bridge::Magic,bridge::Version,total,1},format,v.packet,v.firstBytes+v.secondBytes,0,0};
        RtlCopyMemory(output,&packet,sizeof(packet));
        auto pcm=static_cast<uint8_t*>(output)+sizeof(packet);
        RtlCopyMemory(pcm,v.first,v.firstBytes);
        if(v.secondBytes)RtlCopyMemory(pcm+v.firstBytes,v.second,v.secondBytes);
        copied=total;
    }
    EnterPassive();if(timeline.finishRead(v.packet.sequence,capacity>=total) && capacity>=total)deliveredFrames+=v.packet.frames;
    if(!timeline.prepared())Archive();KeSetEvent(&readIdle_,IO_NO_INCREMENT,FALSE);Leave();
    return capacity>=total?STATUS_SUCCESS:STATUS_BUFFER_TOO_SMALL;
}
void PcmCommit::EnterPassive(){PAGED_CODE();LARGE_INTEGER retry;retry.QuadPart=-10000;while(!TryEnter())KeDelayExecutionThread(KernelMode,FALSE,&retry);}
void PcmCommit::Leave(uint64_t qpc){
    // Only a bounded cache metadata copy is protected by the spin lock.
    // PCM copies use the nonblocking operation gate; realtime callers never
    // wait for another owner or for a consumer's lease.
    if(qpc){KIRQL old;KeAcquireSpinLock(&lock,&old);cachedFrames_=timeline.frames();cachedPackets_=timeline.packetCount();cachedQpc_=qpc;cachedClock_=timeline.clockSnapshot();KeReleaseSpinLock(&lock,old);}
    operation_.leave();
}
void PcmCommit::PositionCache(uint64_t& frames,uint64_t& packets,uint64_t& qpc){KIRQL old;KeAcquireSpinLock(&lock,&old);frames=cachedFrames_;packets=cachedPackets_;qpc=cachedQpc_;KeReleaseSpinLock(&lock,old);}
void InitializePcmOwners(){KeInitializeSpinLock(&ownerLock);}
bool RegisterPcmOwner(PcmCommit* candidate){KIRQL old;KeAcquireSpinLock(&ownerLock,&old);const bool available=!owner;if(available){owner=candidate;ownerDraining=false;candidate->MarkRegistered();}KeReleaseSpinLock(&ownerLock,old);return available;}
void UnregisterPcmOwner(PcmCommit* candidate){
    KIRQL old;KeAcquireSpinLock(&ownerLock,&old);if(owner==candidate)ownerDraining=true;KeReleaseSpinLock(&ownerLock,old);
    ExWaitForRundownProtectionRelease(&candidate->rundown);candidate->Stop();
    // Preserve generation completion order before making another owner eligible.
    KeAcquireSpinLock(&ownerLock,&old);if(owner==candidate){owner=nullptr;ownerDraining=false;}KeReleaseSpinLock(&ownerLock,old);
}
PcmCommit* AcquirePcmOwner(){KIRQL old;KeAcquireSpinLock(&ownerLock,&old);auto selected=ownerDraining?nullptr:owner;if(selected && !ExAcquireRundownProtection(&selected->rundown))selected=nullptr;KeReleaseSpinLock(&ownerLock,old);return selected;}
void ReleasePcmOwner(PcmCommit* selected){ExReleaseRundownProtection(&selected->rundown);}
void GetPcmEvidence(uint64_t generation,const Totals& current,Totals& lifetime,Totals& ended){
    KIRQL old;KeAcquireSpinLock(&ownerLock,&old);lifetime=evidence.lifetime();ended=evidence.ended();
    if(generation>evidence.generation()){lifetime.lostFrames+=current.lostFrames;lifetime.cancelledFrames+=current.cancelledFrames;}
    KeReleaseSpinLock(&ownerLock,old);
}
