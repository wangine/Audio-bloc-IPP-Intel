#include "PcmCommit.h"
using namespace entropy::music;
namespace {constexpr ULONG Tag='2RPM';KSPIN_LOCK ownerLock;PcmCommit* owner=nullptr;}
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
PcmCommit::PcmCommit() noexcept {KeInitializeSpinLock(&lock);ExInitializeRundownProtection(&rundown);KeInitializeEvent(&readIdle_,NotificationEvent,TRUE);}
PcmCommit::~PcmCommit() {
    Stop();
    if(pendingBytes_)ExFreePoolWithTag(pendingBytes_,Tag);
    if(readyBytes_)ExFreePoolWithTag(readyBytes_,Tag);
    if(pendingSlots_)ExFreePoolWithTag(pendingSlots_,Tag);
    if(readySlots_)ExFreePoolWithTag(readySlots_,Tag);
}
NTSTATUS PcmCommit::Prepare(const PcmFormat& f,uint64_t generation,uint64_t frequency,uint8_t* dma,uint32_t frames,uint32_t notifications) {
    PAGED_CODE();
    EnterPassive();
    struct Lease {PcmCommit& pcm;~Lease(){pcm.Leave();}} lease{*this};
    if(timeline.prepared())return STATUS_INVALID_DEVICE_STATE;
    if(!pendingBytes_) {
        pendingBytes_=static_cast<uint8_t*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,static_cast<SIZE_T>(frames)*f.blockAlign,Tag));
        readyBytes_=static_cast<uint8_t*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,static_cast<SIZE_T>(RingFrames)*f.blockAlign,Tag));
        pendingSlots_=static_cast<RingSlot*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,64*sizeof(RingSlot),Tag));
        readySlots_=static_cast<RingSlot*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,RingDescriptors*sizeof(RingSlot),Tag));
        if(!pendingBytes_ || !readyBytes_ || !pendingSlots_ || !readySlots_)return STATUS_INSUFFICIENT_RESOURCES;
        if(!pending_.init(pendingBytes_,frames,f.blockAlign,pendingSlots_,64) || !ready_.init(readyBytes_,RingFrames,f.blockAlign,readySlots_,RingDescriptors))return STATUS_INVALID_PARAMETER;
    }
    if(!timeline.prepare(f,generation,frequency,dma,frames,notifications,pending_,ready_))return STATUS_INVALID_PARAMETER;
    deliveredFrames=0;return STATUS_SUCCESS;
}
void PcmCommit::Stop() {
    PAGED_CODE();
    EnterPassive();const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;
    const bool complete=timeline.requestStop(qpc);Leave(qpc);
    if(!complete)KeWaitForSingleObject(&readIdle_,Executive,KernelMode,FALSE,nullptr);
}
NTSTATUS PcmCommit::Read(void* output,uint32_t capacity,uint32_t& copied) {
    PAGED_CODE();copied=0;PacketView v{};
    if(!TryEnter())return STATUS_DEVICE_NOT_READY;
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
    KeSetEvent(&readIdle_,IO_NO_INCREMENT,FALSE);Leave();
    return capacity>=total?STATUS_SUCCESS:STATUS_BUFFER_TOO_SMALL;
}
void PcmCommit::EnterPassive(){PAGED_CODE();LARGE_INTEGER retry;retry.QuadPart=-10000;while(!TryEnter())KeDelayExecutionThread(KernelMode,FALSE,&retry);}
void PcmCommit::Leave(uint64_t qpc){
    // Only a bounded cache metadata copy is protected by the spin lock.
    // PCM copies use the nonblocking operation gate; realtime callers never
    // wait for another owner or for a consumer's lease.
    if(qpc){KIRQL old;KeAcquireSpinLock(&lock,&old);cachedFrames_=timeline.frames();cachedPackets_=timeline.packetCount();cachedQpc_=qpc;KeReleaseSpinLock(&lock,old);}
    operation_.leave();
}
void PcmCommit::PositionCache(uint64_t& frames,uint64_t& packets,uint64_t& qpc){KIRQL old;KeAcquireSpinLock(&lock,&old);frames=cachedFrames_;packets=cachedPackets_;qpc=cachedQpc_;KeReleaseSpinLock(&lock,old);}
void InitializePcmOwners(){KeInitializeSpinLock(&ownerLock);}
bool RegisterPcmOwner(PcmCommit* candidate){KIRQL old;KeAcquireSpinLock(&ownerLock,&old);const bool available=!owner;if(available)owner=candidate;KeReleaseSpinLock(&ownerLock,old);return available;}
void UnregisterPcmOwner(PcmCommit* candidate){KIRQL old;KeAcquireSpinLock(&ownerLock,&old);if(owner==candidate)owner=nullptr;KeReleaseSpinLock(&ownerLock,old);ExWaitForRundownProtectionRelease(&candidate->rundown);}
PcmCommit* AcquirePcmOwner(){KIRQL old;KeAcquireSpinLock(&ownerLock,&old);auto selected=owner;if(selected && !ExAcquireRundownProtection(&selected->rundown))selected=nullptr;KeReleaseSpinLock(&ownerLock,old);return selected;}
void ReleasePcmOwner(PcmCommit* selected){ExReleaseRundownProtection(&selected->rundown);}
