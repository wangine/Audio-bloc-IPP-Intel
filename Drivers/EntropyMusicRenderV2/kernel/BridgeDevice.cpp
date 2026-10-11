#include "BridgeDevice.h"
#include "PcmCommit.h"
using namespace entropy::music;
namespace {
constexpr ULONG Tag='2BRM';
const GUID InterfaceGuid={0xab93289e,0x2692,0x47a9,{0xbc,0x47,0xed,0x17,0x11,0xf6,0xe4,0x30}};
UNICODE_STRING interfaceName{};
KSPIN_LOCK sessionsLock;
PDRIVER_DISPATCH original[IRP_MJ_MAXIMUM_FUNCTION+1]{};
int fileMarker=0;
PFILE_OBJECT reader=nullptr;
ULONG sessions=0;
volatile LONG enabled=0;
volatile LONG64 traceCounters[8]{};
volatile LONG lastWrite[4]{};
struct Session {volatile LONG closing;uint64_t nonce;};
NTSTATUS Complete(PIRP irp,NTSTATUS status,ULONG bytes=0){irp->IoStatus.Status=status;irp->IoStatus.Information=bytes;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;}
bool HeaderIs(const bridge::Header& h,ULONG length,uint32_t kind,ULONG expected){return length==expected && h.magic==bridge::Magic && h.version==bridge::Version && h.totalBytes==expected && h.kind==kind;}
NTSTATUS PrivateDispatch(PDEVICE_OBJECT device,PIRP irp) {
    auto stack=IoGetCurrentIrpStackLocation(irp);const auto file=stack->FileObject;
    if(stack->MajorFunction==IRP_MJ_CREATE && file) {
        UNICODE_STRING reference=RTL_CONSTANT_STRING(L"\\EntropyMusicPcmV1");
        if(RtlEqualUnicodeString(&file->FileName,&reference,TRUE)) {
            if(!InterlockedCompareExchange(&enabled,0,0))return Complete(irp,STATUS_DEVICE_NOT_CONNECTED);
            auto session=static_cast<Session*>(ExAllocatePool2(POOL_FLAG_NON_PAGED,sizeof(Session),Tag));
            if(!session)return Complete(irp,STATUS_INSUFFICIENT_RESOURCES);
            KIRQL old;KeAcquireSpinLock(&sessionsLock,&old);if(sessions>=16){KeReleaseSpinLock(&sessionsLock,old);ExFreePoolWithTag(session,Tag);return Complete(irp,STATUS_DEVICE_BUSY);}++sessions;KeReleaseSpinLock(&sessionsLock,old);
            file->FsContext=session;file->FsContext2=&fileMarker;return Complete(irp,STATUS_SUCCESS);
        }
    }
    if(!file || file->FsContext2!=&fileMarker)return original[stack->MajorFunction](device,irp);
    auto session=static_cast<Session*>(file->FsContext);
    if(!session)return Complete(irp,STATUS_INVALID_HANDLE);
    if(stack->MajorFunction==IRP_MJ_CLEANUP || stack->MajorFunction==IRP_MJ_CLOSE) {
        InterlockedExchange(&session->closing,1);KIRQL old;KeAcquireSpinLock(&sessionsLock,&old);if(reader==file)reader=nullptr;
        if(stack->MajorFunction==IRP_MJ_CLOSE)--sessions;KeReleaseSpinLock(&sessionsLock,old);
        if(stack->MajorFunction==IRP_MJ_CLOSE){file->FsContext=file->FsContext2=nullptr;ExFreePoolWithTag(session,Tag);}
        return Complete(irp,STATUS_SUCCESS);
    }
    if(stack->MajorFunction!=IRP_MJ_DEVICE_CONTROL)return Complete(irp,STATUS_INVALID_DEVICE_REQUEST);
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return Complete(irp,STATUS_INVALID_DEVICE_STATE);
    if(irp->Cancel || session->closing)return Complete(irp,STATUS_CANCELLED);
    if(!InterlockedCompareExchange(&enabled,0,0))return Complete(irp,STATUS_DEVICE_NOT_CONNECTED);
    const auto input=stack->Parameters.DeviceIoControl.InputBufferLength,output=stack->Parameters.DeviceIoControl.OutputBufferLength;
    const auto code=stack->Parameters.DeviceIoControl.IoControlCode;
    auto buffer=irp->AssociatedIrp.SystemBuffer;
    if(!buffer || input<sizeof(bridge::Header))return Complete(irp,STATUS_INVALID_PARAMETER);
    bridge::Header header;RtlCopyMemory(&header,buffer,sizeof(header));
    if(code==bridge::IoctlClaim) {
        if(!HeaderIs(header,input,3,sizeof(bridge::Claim)) || output)return Complete(irp,STATUS_INVALID_PARAMETER);
        bridge::Claim claim;RtlCopyMemory(&claim,buffer,sizeof(claim));if(!claim.nonce || claim.reserved)return Complete(irp,STATUS_INVALID_PARAMETER);
        KIRQL old;KeAcquireSpinLock(&sessionsLock,&old);const bool available=!reader || reader==file;
        if(available){reader=file;session->nonce=claim.nonce;}KeReleaseSpinLock(&sessionsLock,old);return Complete(irp,available?STATUS_SUCCESS:STATUS_DEVICE_BUSY);
    }
    if(code==bridge::IoctlRead) {
        if(!HeaderIs(header,input,1,sizeof(header)) || output<sizeof(bridge::Packet) || output>sizeof(bridge::Packet)+MaxPacketFrames*8)return Complete(irp,STATUS_INVALID_PARAMETER);
        KIRQL old;KeAcquireSpinLock(&sessionsLock,&old);const bool own=reader==file;KeReleaseSpinLock(&sessionsLock,old);
        if(!own)return Complete(irp,STATUS_ACCESS_DENIED);
        auto pcm=AcquirePcmOwner();if(!pcm)return Complete(irp,STATUS_DEVICE_NOT_READY);
        uint32_t copied=0;const auto status=pcm->Read(buffer,output,copied);ReleasePcmOwner(pcm);
        return Complete(irp,status,copied);
    }
    if(code==bridge::IoctlInfo) {
        if(!HeaderIs(header,input,2,sizeof(header)) || output!=sizeof(bridge::Info))return Complete(irp,STATUS_INVALID_PARAMETER);
        bridge::Info info{};info.header={bridge::Magic,bridge::Version,sizeof(info),2};RtlCopyMemory(info.interfaceGuid,bridge::InterfaceGuid,16);info.driverBuild=0x00030004;
        KIRQL old;KeAcquireSpinLock(&sessionsLock,&old);info.readerAttached=reader?1:0;KeReleaseSpinLock(&sessionsLock,old);
        Totals lifetime{},ended{};GetPcmEvidence(0,{},lifetime,ended);
        info.lifetimeLost=lifetime.lostFrames;info.lifetimeCancelled=lifetime.cancelledFrames;info.terminatedLost=ended.lostFrames;info.terminatedCancelled=ended.cancelledFrames;
        auto pcm=AcquirePcmOwner();if(pcm) {
            if(pcm->TryEnter()) {
                pcm->Flush();const auto& t=pcm->timeline;const auto totals=pcm->CurrentTotals();GetPcmEvidence(t.generation(),totals,lifetime,ended);
                info.format=t.format();info.generation=t.generation();info.qpcFrequency=t.frequency();info.deliveredFrames=pcm->deliveredFrames;info.allocationMode=static_cast<uint32_t>(t.owner());
                info.state=pcm->fault()?4:(t.stopping()?3:(t.running()?2:(t.prepared()?1:0)));info.sourceFrames=t.frames();info.packetCount=t.packetCount();
                info.committedFrames=totals.committedFrames;info.publishedFrames=totals.publishedFrames;info.lostFrames=totals.lostFrames;info.cancelledFrames=totals.cancelledFrames;
                info.currentUnderrunFrames=t.uncommittedFrames()+(t.frames()>t.committedEnd()?t.frames()-t.committedEnd():0);info.lifetimeLost=lifetime.lostFrames;info.lifetimeCancelled=lifetime.cancelledFrames;
                info.terminatedLost=ended.lostFrames;info.terminatedCancelled=ended.cancelledFrames;
                pcm->Leave();
            } else {ReleasePcmOwner(pcm);return Complete(irp,STATUS_DEVICE_NOT_READY);}
            ReleasePcmOwner(pcm);
        }
        RtlCopyMemory(buffer,&info,sizeof(info));return Complete(irp,STATUS_SUCCESS,sizeof(info));
    }
    if(code==bridge::IoctlTrace) {
        if(!HeaderIs(header,input,4,sizeof(header)) || output!=sizeof(bridge::Trace))return Complete(irp,STATUS_INVALID_PARAMETER);
        bridge::Trace trace{};trace.header={bridge::Magic,bridge::Version,sizeof(trace),4};
        for(uint32_t i=0;i<8;++i)trace.counters[i]=InterlockedCompareExchange64(&traceCounters[i],0,0);
        trace.lastPacketNumber=InterlockedCompareExchange(&lastWrite[0],0,0);trace.lastFlags=InterlockedCompareExchange(&lastWrite[1],0,0);
        trace.lastEosBytes=InterlockedCompareExchange(&lastWrite[2],0,0);trace.lastStatus=InterlockedCompareExchange(&lastWrite[3],0,0);
        RtlCopyMemory(buffer,&trace,sizeof(trace));return Complete(irp,STATUS_SUCCESS,sizeof(trace));
    }
    return Complete(irp,STATUS_INVALID_DEVICE_REQUEST);
}
}
void TraceMusicEvent(bridge::TraceEvent event) noexcept{InterlockedIncrement64(&traceCounters[static_cast<uint32_t>(event)]);}
NTSTATUS TraceMusicWrite(ULONG number,DWORD flags,ULONG eosBytes,NTSTATUS status) noexcept{
    InterlockedExchange(&lastWrite[0],number);InterlockedExchange(&lastWrite[1],flags);InterlockedExchange(&lastWrite[2],eosBytes);InterlockedExchange(&lastWrite[3],status);
    TraceMusicEvent(NT_SUCCESS(status)?bridge::TraceEvent::WriteSuccess:bridge::TraceEvent::WriteFailure);return status;
}
void InitializeBridgeDispatch(PDRIVER_OBJECT driver) {
    KeInitializeSpinLock(&sessionsLock);
    const ULONG majors[]={IRP_MJ_CREATE,IRP_MJ_CLEANUP,IRP_MJ_CLOSE,IRP_MJ_DEVICE_CONTROL};
    for(auto major:majors){original[major]=driver->MajorFunction[major];driver->MajorFunction[major]=PrivateDispatch;}
}
NTSTATUS RegisterBridgeInterface(PDEVICE_OBJECT physical) {
    PAGED_CODE();UNICODE_STRING reference=RTL_CONSTANT_STRING(L"EntropyMusicPcmV1");
    if(!interfaceName.Buffer){auto status=IoRegisterDeviceInterface(physical,&InterfaceGuid,&reference,&interfaceName);if(!NT_SUCCESS(status))return status;}
    const auto status=IoSetDeviceInterfaceState(&interfaceName,TRUE);if(NT_SUCCESS(status))InterlockedExchange(&enabled,1);return status;
}
void DisableBridgeInterface(){PAGED_CODE();InterlockedExchange(&enabled,0);if(interfaceName.Buffer)IoSetDeviceInterfaceState(&interfaceName,FALSE);}
void ShutdownBridgeInterface(){DisableBridgeInterface();if(interfaceName.Buffer){RtlFreeUnicodeString(&interfaceName);interfaceName={};}}
