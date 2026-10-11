/* Copyright (c) Microsoft Corporation. Derived WaveRT shell, MS-PL.
   PCM ownership/timeline implementation Copyright (c) Entropy Audio. */
#include "definitions.h"
#include "endpoints.h"
#include "minwavert.h"
#include "minwavertstream.h"
#include "../AllocationPolicy.h"
#include "../BridgeDevice.h"
using namespace entropy::music;
namespace {constexpr ULONG Tag='2SWM';volatile LONG64 generations=0;}
CMiniportWaveRTStream::~CMiniportWaveRTStream() {
    PAGED_CODE();
    if(m_pNotificationTimer){ExDeleteTimer(m_pNotificationTimer,TRUE,TRUE,nullptr);m_pNotificationTimer=nullptr;}
    if(m_Pcm){if(m_Registered)UnregisterPcmOwner(m_Pcm);delete m_Pcm;m_Pcm=nullptr;}
    while(!IsListEmpty(&m_NotificationList)){auto p=CONTAINING_RECORD(RemoveHeadList(&m_NotificationList),NotificationListEntry,ListEntry);ExFreePoolWithTag(p,Tag);}
    if(m_pMiniport){if(m_bUnregisterStream)m_pMiniport->StreamClosed(m_ulPin,this);m_pMiniport->Release();}
}
NTSTATUS CMiniportWaveRTStream::Init(PCMiniportWaveRT mini,PPORTWAVERTSTREAM port,ULONG pin,BOOLEAN capture,PKSDATAFORMAT data,GUID mode) {
    PAGED_CODE();InitializeListHead(&m_NotificationList);KeInitializeSpinLock(&m_EventLock);
    if(capture || pin!=0 || !ParseMusicFormat(data,m_Format))return STATUS_NO_MATCH;
    m_pMiniport=mini;m_pMiniport->AddRef();m_pPortStream=port;m_ulPin=pin;m_SignalProcessingMode=mode;
    LARGE_INTEGER frequency;KeQueryPerformanceCounter(&frequency);m_Frequency=frequency.QuadPart;
    m_pNotificationTimer=ExAllocateTimer(TimerNotifyRT,this,EX_TIMER_HIGH_RESOLUTION);
    if(!m_pNotificationTimer)return STATUS_INSUFFICIENT_RESOURCES;
    const auto status=m_pMiniport->StreamCreated(pin,this);if(NT_SUCCESS(status))m_bUnregisterStream=TRUE;return status;
}
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::NonDelegatingQueryInterface(REFIID iid,PVOID* object) {
    PAGED_CODE();if(!object)return STATUS_INVALID_PARAMETER;*object=nullptr;
    if(IsEqualGUID(iid,IID_IUnknown) || IsEqualGUID(iid,IID_IMiniportWaveRTStream))*object=static_cast<IMiniportWaveRTStream*>(this);
    else if(IsEqualGUID(iid,IID_IMiniportWaveRTStreamNotification))*object=static_cast<IMiniportWaveRTStreamNotification*>(this);
    else if(IsEqualGUID(iid,IID_IMiniportWaveRTOutputStream)){TraceMusicEvent(bridge::TraceEvent::OutputInterface);*object=static_cast<IMiniportWaveRTOutputStream*>(this);}
    else if(IsEqualGUID(iid,IID_IDrmAudioStream))*object=static_cast<IDrmAudioStream*>(this);
    if(!*object)return STATUS_NOINTERFACE;static_cast<PUNKNOWN>(*object)->AddRef();return STATUS_SUCCESS;
}
NTSTATUS CMiniportWaveRTStream::EnsurePrepared() {
    if(!m_Pcm || !m_pDmaBuffer)return STATUS_INVALID_DEVICE_STATE;
    if(m_Pcm->timeline.prepared())return STATUS_SUCCESS;
    m_LastNotified=0;m_EosNotified=false;
    return m_Pcm->Prepare(m_Format,InterlockedIncrement64(&generations),m_Frequency,m_pDmaBuffer,m_ulDmaBufferSize/m_Format.blockAlign,m_ulNotificationsPerBuffer);
}
NTSTATUS CMiniportWaveRTStream::Allocate(ULONG size,ULONG notifications,PMDL* mdl,ULONG* actual,ULONG* offset,MEMORY_CACHING_TYPE* cache) {
    PAGED_CODE();
    // Elapsed QPC does not prove ownership of position-only DMA bytes.
    if(!rawAllocationSupported(notifications))return STATUS_NOT_SUPPORTED;
    if(!mdl || !actual || !offset || !cache || m_pDmaBuffer || !size || notifications>2 || size%m_Format.blockAlign ||
        (notifications && size%(notifications*m_Format.blockAlign)) || size/m_Format.blockAlign>MaxPacketFrames*(notifications?notifications:1))return STATUS_INVALID_PARAMETER;
    *mdl=nullptr;*actual=0;
    if(size/(notifications*m_Format.blockAlign)<minimumPacketFrames(m_Format.sampleRate))return STATUS_NOT_SUPPORTED;
    if(!m_pNotificationTimer){m_pNotificationTimer=ExAllocateTimer(TimerNotifyRT,this,EX_TIMER_HIGH_RESOLUTION);if(!m_pNotificationTimer)return STATUS_INSUFFICIENT_RESOURCES;}
    PHYSICAL_ADDRESS high;high.QuadPart=MAXULONG;
    auto allocated=m_pPortStream->AllocatePagesForMdl(high,size);if(!allocated)return STATUS_INSUFFICIENT_RESOURCES;
    m_pDmaBuffer=static_cast<BYTE*>(m_pPortStream->MapAllocatedPages(allocated,MmCached));
    if(!m_pDmaBuffer){m_pPortStream->FreePagesFromMdl(allocated);return STATUS_INSUFFICIENT_RESOURCES;}
    m_ulDmaBufferSize=size;m_ulNotificationsPerBuffer=notifications;
    m_Pcm=new(POOL_FLAG_NON_PAGED,Tag) PcmCommit;
    auto status=m_Pcm?EnsurePrepared():STATUS_INSUFFICIENT_RESOURCES;
    if(NT_SUCCESS(status) && !RegisterPcmOwner(m_Pcm))status=STATUS_DEVICE_BUSY;
    if(!NT_SUCCESS(status)){Free(allocated);return status;}
    m_Registered=true;*mdl=allocated;*actual=size;*offset=0;*cache=MmCached;return STATUS_SUCCESS;
}
NTSTATUS CMiniportWaveRTStream::AllocateAudioBuffer(ULONG size,PMDL* mdl,ULONG* actual,ULONG* offset,MEMORY_CACHING_TYPE* cache){return Allocate(size,0,mdl,actual,offset,cache);}
NTSTATUS CMiniportWaveRTStream::AllocateBufferWithNotification(ULONG notifications,ULONG size,PMDL* mdl,ULONG* actual,ULONG* offset,MEMORY_CACHING_TYPE* cache){if(!notifications)return STATUS_INVALID_PARAMETER;return Allocate(size,notifications,mdl,actual,offset,cache);}
void CMiniportWaveRTStream::Free(PMDL mdl) {
    PAGED_CODE();if(m_pNotificationTimer){ExDeleteTimer(m_pNotificationTimer,TRUE,TRUE,nullptr);m_pNotificationTimer=nullptr;}
    if(m_Pcm){if(m_Registered){UnregisterPcmOwner(m_Pcm);m_Registered=false;}delete m_Pcm;m_Pcm=nullptr;}
    if(mdl){if(m_pDmaBuffer)m_pPortStream->UnmapAllocatedPages(m_pDmaBuffer,mdl);m_pPortStream->FreePagesFromMdl(mdl);}
    m_pDmaBuffer=nullptr;m_ulDmaBufferSize=m_ulNotificationsPerBuffer=0;
}
void CMiniportWaveRTStream::FreeAudioBuffer(PMDL mdl,ULONG size){UNREFERENCED_PARAMETER(size);Free(mdl);}
void CMiniportWaveRTStream::FreeBufferWithNotification(PMDL mdl,ULONG size){UNREFERENCED_PARAMETER(size);Free(mdl);}
NTSTATUS CMiniportWaveRTStream::SetState(KSSTATE state) {
    PAGED_CODE();if(state<KSSTATE_STOP || state>KSSTATE_RUN)return STATUS_INVALID_PARAMETER;
    if(state==m_KsState)return STATUS_SUCCESS;
    if(state!=KSSTATE_RUN && m_pNotificationTimer){ExCancelTimer(m_pNotificationTimer,nullptr);KeFlushQueuedDpcs();}
    if(state==KSSTATE_STOP){if(m_Pcm)m_Pcm->Stop();m_KsState=state;return STATUS_SUCCESS;}
    if(state==KSSTATE_ACQUIRE || state==KSSTATE_PAUSE){
        if(m_Pcm && !m_Pcm->timeline.prepared()){auto status=EnsurePrepared();if(!NT_SUCCESS(status))return status;}
        if(m_Pcm && !m_Pcm->SetRunning(false))return STATUS_INVALID_DEVICE_STATE;
        m_KsState=state;return STATUS_SUCCESS;
    }
    auto status=EnsurePrepared();if(!NT_SUCCESS(status))return status;
    const bool started=m_Pcm->SetRunning(true);
    if(!started)return STATUS_INVALID_DEVICE_STATE;m_KsState=state;
    if(m_ulNotificationsPerBuffer)ExSetTimer(m_pNotificationTimer,-10000,10000,nullptr);
    return STATUS_SUCCESS;
}
NTSTATUS CMiniportWaveRTStream::SetWritePacket(ULONG number,DWORD flags,ULONG eosBytes) {
    TraceMusicEvent(bridge::TraceEvent::WriteCall);
    if(!m_ulNotificationsPerBuffer)return STATUS_NOT_SUPPORTED;
    if(flags&~KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM)return STATUS_INVALID_PARAMETER;
    // Prepare before prefill, including a new generation after STOP.
    if(!m_Pcm)return STATUS_INVALID_DEVICE_STATE;
    const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;
    const auto status=m_Pcm->Submit(number,(flags&KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM)!=0,eosBytes,qpc);
    TraceMusicEvent(NT_SUCCESS(status)?bridge::TraceEvent::WriteSuccess:bridge::TraceEvent::WriteFailure);return status;
}
NTSTATUS CMiniportWaveRTStream::GetPosition(KSAUDIO_POSITION* position) {
    TraceMusicEvent(bridge::TraceEvent::Position);
    if(!position)return STATUS_INVALID_PARAMETER;position->PlayOffset=position->WriteOffset=0;if(!m_Pcm)return STATUS_SUCCESS;
    uint64_t frames=0,packets=0,qpc=0;bool ok=true;
    if(m_Pcm->TryEnter()){qpc=KeQueryPerformanceCounter(nullptr).QuadPart;m_Pcm->Flush();ok=!m_Pcm->timeline.prepared() || m_Pcm->timeline.advance(qpc,true);frames=m_Pcm->timeline.frames();m_Pcm->Leave(qpc);}
    else m_Pcm->PositionCache(frames,packets,qpc);
    const auto offset=(frames%(m_ulDmaBufferSize/m_Format.blockAlign))*m_Format.blockAlign;
    position->PlayOffset=position->WriteOffset=offset;return ok?STATUS_SUCCESS:STATUS_DATA_ERROR;
}
NTSTATUS CMiniportWaveRTStream::GetPresentationPosition(KSAUDIO_PRESENTATION_POSITION* position) {
    TraceMusicEvent(bridge::TraceEvent::Presentation);
    if(!position)return STATUS_INVALID_PARAMETER;RtlZeroMemory(position,sizeof(*position));if(!m_Pcm)return STATUS_SUCCESS;
    uint64_t frames=0,packets=0,qpc=0;bool ok=true;
    if(m_Pcm->TryEnter()){qpc=KeQueryPerformanceCounter(nullptr).QuadPart;m_Pcm->Flush();ok=!m_Pcm->timeline.prepared() || m_Pcm->timeline.advance(qpc,true);frames=m_Pcm->timeline.frames();m_Pcm->Leave(qpc);}
    else m_Pcm->PositionCache(frames,packets,qpc);
    position->u64PositionInBlocks=frames;position->u64QPCPosition=qpc;return ok?STATUS_SUCCESS:STATUS_DATA_ERROR;
}
NTSTATUS CMiniportWaveRTStream::GetOutputStreamPresentationPosition(KSAUDIO_PRESENTATION_POSITION* p){if(!m_ulNotificationsPerBuffer)return STATUS_NOT_SUPPORTED;return GetPresentationPosition(p);}
NTSTATUS CMiniportWaveRTStream::GetPacketCount(ULONG* count) {
    TraceMusicEvent(bridge::TraceEvent::PacketCount);
    if(!count)return STATUS_INVALID_PARAMETER;if(!m_ulNotificationsPerBuffer)return STATUS_NOT_SUPPORTED;
    uint64_t frames=0,packets=0,qpc=0;
    if(m_Pcm->TryEnter()){qpc=KeQueryPerformanceCounter(nullptr).QuadPart;m_Pcm->Flush();if(m_Pcm->timeline.prepared())m_Pcm->timeline.advance(qpc,false);packets=m_Pcm->timeline.packetCount();m_Pcm->Leave(qpc);}
    else m_Pcm->PositionCache(frames,packets,qpc);
    *count=static_cast<ULONG>(packets);return STATUS_SUCCESS;
}
NTSTATUS CMiniportWaveRTStream::RegisterNotificationEvent(PKEVENT event) {
    PAGED_CODE();if(!event || !m_Pcm)return STATUS_INVALID_PARAMETER;
    auto entry=static_cast<NotificationListEntry*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(NotificationListEntry),Tag));if(!entry)return STATUS_INSUFFICIENT_RESOURCES;
    entry->NotificationEvent=event;KIRQL old;KeAcquireSpinLock(&m_EventLock,&old);
    for(auto p=m_NotificationList.Flink;p!=&m_NotificationList;p=p->Flink){if(CONTAINING_RECORD(p,NotificationListEntry,ListEntry)->NotificationEvent==event){KeReleaseSpinLock(&m_EventLock,old);ExFreePoolWithTag(entry,Tag);return STATUS_OBJECT_NAME_COLLISION;}}
    InsertTailList(&m_NotificationList,&entry->ListEntry);KeReleaseSpinLock(&m_EventLock,old);return STATUS_SUCCESS;
}
NTSTATUS CMiniportWaveRTStream::UnregisterNotificationEvent(PKEVENT event) {
    PAGED_CODE();if(!m_Pcm)return STATUS_NOT_FOUND;NotificationListEntry* found=nullptr;KIRQL old;KeAcquireSpinLock(&m_EventLock,&old);
    for(auto p=m_NotificationList.Flink;p!=&m_NotificationList;p=p->Flink){auto e=CONTAINING_RECORD(p,NotificationListEntry,ListEntry);if(e->NotificationEvent==event){RemoveEntryList(p);found=e;break;}}
    KeReleaseSpinLock(&m_EventLock,old);if(found){ExFreePoolWithTag(found,Tag);return STATUS_SUCCESS;}return STATUS_NOT_FOUND;
}
NTSTATUS CMiniportWaveRTStream::GetClockRegister(PKSRTAUDIO_HWREGISTER p){UNREFERENCED_PARAMETER(p);return STATUS_NOT_IMPLEMENTED;}
NTSTATUS CMiniportWaveRTStream::GetPositionRegister(PKSRTAUDIO_HWREGISTER p){UNREFERENCED_PARAMETER(p);return STATUS_NOT_IMPLEMENTED;}
void CMiniportWaveRTStream::GetHWLatency(PKSRTAUDIO_HWLATENCY p){RtlZeroMemory(p,sizeof(*p));}
NTSTATUS CMiniportWaveRTStream::SetFormat(KSDATAFORMAT* p){UNREFERENCED_PARAMETER(p);return STATUS_NOT_SUPPORTED;}
STDMETHODIMP_(NTSTATUS) CMiniportWaveRTStream::SetContentId(ULONG content,PCDRMRIGHTS rights) {
    PAGED_CODE();if(!rights)return STATUS_INVALID_PARAMETER;
    // Private raw export is not a DRM-authenticated protected path.
    if(rights->CopyProtect)return STATUS_NOT_SUPPORTED;m_ulContentId=content;return m_pMiniport->UpdateDrmRights();
}
void TimerNotifyRT(PEX_TIMER timer,PVOID context) {
    UNREFERENCED_PARAMETER(timer);auto self=static_cast<CMiniportWaveRTStream*>(context);if(!self || !self->m_Pcm)return;
    auto pcm=self->m_Pcm;if(!pcm->TryEnter())return;const auto qpc=KeQueryPerformanceCounter(nullptr).QuadPart;
    bool notify=false,eos=false;
    pcm->Flush();
    if(pcm->timeline.running() && pcm->timeline.advance(qpc,false)) {
        const auto count=pcm->timeline.packetCount();
        eos=pcm->timeline.eos() && pcm->timeline.frames()>=pcm->timeline.committedEnd();
        if(count>self->m_LastNotified || (eos && !self->m_EosNotified)) {
            self->m_LastNotified=count;self->m_EosNotified=eos;
            notify=true;
        }
    }
    pcm->Leave(qpc);
    if(notify){TraceMusicEvent(bridge::TraceEvent::Notification);KIRQL old;KeAcquireSpinLock(&self->m_EventLock,&old);for(auto p=self->m_NotificationList.Flink;p!=&self->m_NotificationList;p=p->Flink)KeSetEvent(CONTAINING_RECORD(p,NotificationListEntry,ListEntry)->NotificationEvent,IO_NO_INCREMENT,FALSE);KeReleaseSpinLock(&self->m_EventLock,old);}
    if(eos)ExCancelTimer(self->m_pNotificationTimer,nullptr);
}
