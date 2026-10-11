/* Copyright (c) Microsoft Corporation. Derived WaveRT shell, MS-PL.
   PCM ownership/timeline implementation Copyright (c) Entropy Audio. */
#pragma once
#include "../PcmCommit.h"
struct NotificationListEntry {LIST_ENTRY ListEntry;PKEVENT NotificationEvent;};
EXT_CALLBACK TimerNotifyRT;
class CMiniportWaveRT;
typedef CMiniportWaveRT* PCMiniportWaveRT;
class CMiniportWaveRTStream:public IDrmAudioStream,public IMiniportWaveRTStreamNotification,public IMiniportWaveRTOutputStream,public CUnknown {
    PPORTWAVERTSTREAM m_pPortStream=nullptr;
    LIST_ENTRY m_NotificationList{};
    PEX_TIMER m_pNotificationTimer=nullptr;
    CMiniportWaveRT* m_pMiniport=nullptr;
    ULONG m_ulPin=0;
    BOOLEAN m_bUnregisterStream=FALSE;
    BYTE* m_pDmaBuffer=nullptr;
    ULONG m_ulDmaBufferSize=0,m_ulNotificationsPerBuffer=0;
    KSSTATE m_KsState=KSSTATE_STOP;
    entropy::music::PcmFormat m_Format{};
    uint64_t m_Frequency=0,m_LastNotified=0;
    PcmCommit* m_Pcm=nullptr;
    bool m_Registered=false,m_EosNotified=false;
    GUID m_SignalProcessingMode{};
    KSPIN_LOCK m_EventLock{};
    NTSTATUS Allocate(ULONG size,ULONG notifications,PMDL*,ULONG*,ULONG*,MEMORY_CACHING_TYPE*);
    NTSTATUS EnsurePrepared();
    void Free(PMDL);
public:
    ULONG m_ulContentId=0;
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTStream);
    ~CMiniportWaveRTStream();
    IMP_IMiniportWaveRTStream;
    IMP_IMiniportWaveRTStreamNotification;
    IMP_IMiniportWaveRTOutputStream;
    IMP_IDrmAudioStream;
    NTSTATUS Init(PCMiniportWaveRT,PPORTWAVERTSTREAM,ULONG,BOOLEAN,PKSDATAFORMAT,GUID);
    NTSTATUS GetPresentationPosition(KSAUDIO_PRESENTATION_POSITION*);
    GUID GetSignalProcessingMode(){return m_SignalProcessingMode;}
    friend class CMiniportWaveRT;
    friend EXT_CALLBACK TimerNotifyRT;
};
