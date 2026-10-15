#pragma once
#include "PcmCommit.h"
namespace entropy::music::kernel_clock {
struct FileState {volatile LONG busy;volatile LONG64 lease;};
// Tokens are identity, not authentication secrets. Access is checked by the
// device ACL and read/write IOCTL rights. Never expose this endpoint remotely.
inline volatile LONG64 nextLease=0;
inline void close(FileState& file) noexcept {
    const auto token=static_cast<uint64_t>(InterlockedExchange64(&file.lease,0));
    if(!token)return;
    auto* pcm=AcquirePcmOwner();if(pcm){pcm->ReleaseClockHandle(token);ReleasePcmOwner(pcm);}
}
inline NTSTATUS dispatch(PIRP irp,FileState& file,volatile LONG& closing,uint32_t code,
                         void* buffer,uint32_t inputBytes,uint32_t outputBytes,ULONG& copied) noexcept {
    copied=0;
    if(!buffer || inputBytes!=sizeof(clockwire::Request) || outputBytes!=sizeof(clockwire::Reply))return STATUS_INVALID_PARAMETER;
    const auto access=IoValidateDeviceIoControlAccess(irp,FILE_READ_DATA|FILE_WRITE_DATA);
    if(!NT_SUCCESS(access))return access;
    clockwire::Request request{};RtlCopyMemory(&request,buffer,sizeof(request));
    if(!clockwire::validate(request,code,inputBytes,outputBytes))return STATUS_INVALID_PARAMETER;
    if(InterlockedCompareExchange(&file.busy,1,0))return STATUS_DEVICE_BUSY;
    struct Busy {FileState& f;~Busy(){InterlockedExchange(&f.busy,0);}} busy{file};
    if(InterlockedCompareExchange(&closing,0,0))return STATUS_CANCELLED;
    const auto kind=clockwire::Kind(request.header.kind);
    auto response=clockwire::reply(kind);
    if(kind!=clockwire::Kind::Capabilities){
        if((kind==clockwire::Kind::Update || kind==clockwire::Kind::Release) &&
           request.lease!=static_cast<uint64_t>(InterlockedCompareExchange64(&file.lease,0,0)))return STATUS_ACCESS_DENIED;
        uint64_t token=0;
        if(kind==clockwire::Kind::Acquire){
            const auto candidate=InterlockedIncrement64(&nextLease);
            if(candidate<=0)return STATUS_INTEGER_OVERFLOW;
            token=static_cast<uint64_t>(candidate);
        }
        auto* pcm=AcquirePcmOwner();
        if(!pcm){if(kind!=clockwire::Kind::Status)return STATUS_DEVICE_NOT_READY;}
        else {
            const auto status=pcm->ClockControl(request,token,response);ReleasePcmOwner(pcm);
            if(!NT_SUCCESS(status))return status;
            if(kind==clockwire::Kind::Acquire)InterlockedExchange64(&file.lease,static_cast<LONG64>(token));
            if(kind==clockwire::Kind::Release)InterlockedExchange64(&file.lease,0);
        }
    }
    if(InterlockedCompareExchange(&closing,0,0)){close(file);return STATUS_CANCELLED;}
    RtlCopyMemory(buffer,&response,sizeof(response));copied=sizeof(response);return STATUS_SUCCESS;
}
}
