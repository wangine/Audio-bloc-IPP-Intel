#include "NativeBridge.h"
namespace entropy::music::provider {
bool NativeBridge::open(const DeviceIdentity& identity){
    if(device_!=INVALID_HANDLE_VALUE || !validIdentity(identity))return false;
    stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stop_)return false;
    device_=CreateFileW(identity.interfacePath.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    if(device_==INVALID_HANDLE_VALUE){close();return false;}
    LARGE_INTEGER qpc;QueryPerformanceCounter(&qpc);
    bridge::Claim claim{{bridge::Magic,bridge::Version,sizeof(claim),3},static_cast<uint64_t>(qpc.QuadPart)|1,0};DWORD bytes=0;
    if(!DeviceIoControl(device_,bridge::IoctlClaim,&claim,sizeof(claim),nullptr,0,&bytes,nullptr)){close();return false;}
    return true;
}
IoResult NativeBridge::read(void* output,uint32_t capacity,uint32_t& copied){
    copied=0;if(device_==INVALID_HANDLE_VALUE)return IoResult::Failure;
    if(WaitForSingleObject(stop_,0)==WAIT_OBJECT_0)return IoResult::Cancelled;
    bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),1};DWORD bytes=0;
    if(DeviceIoControl(device_,bridge::IoctlRead,&request,sizeof(request),output,capacity,&bytes,nullptr)){copied=bytes;return IoResult::Success;}
    const auto error=GetLastError();
    if(error==ERROR_NOT_READY || error==ERROR_BUSY || error==ERROR_NO_DATA){return WaitForSingleObject(stop_,2)==WAIT_OBJECT_0?IoResult::Cancelled:IoResult::NoData;}
    if(error==ERROR_INSUFFICIENT_BUFFER)return IoResult::TooSmall;
    if(error==ERROR_OPERATION_ABORTED)return IoResult::Cancelled;
    return IoResult::Failure;
}
bool NativeBridge::info(bridge::Info& info){
    if(device_==INVALID_HANDLE_VALUE)return false;
    bridge::Header request{bridge::Magic,bridge::Version,sizeof(request),2};DWORD bytes=0;
    return DeviceIoControl(device_,bridge::IoctlInfo,&request,sizeof(request),&info,sizeof(info),&bytes,nullptr) && bytes==sizeof(info);
}
void NativeBridge::cancel(){if(stop_)SetEvent(stop_);if(device_!=INVALID_HANDLE_VALUE)CancelIoEx(device_,nullptr);}
void NativeBridge::close(){if(device_!=INVALID_HANDLE_VALUE){CloseHandle(device_);device_=INVALID_HANDLE_VALUE;}if(stop_){CloseHandle(stop_);stop_=nullptr;}}
}
