#include "AuditObserver.h"
namespace entropy::music::provider {
AuditObserver::AuditObserver(){
    wchar_t path[512]{};const auto length=GetEnvironmentVariableW(L"ENTROPY_MUSIC_AUDIT_PIPE",path,512);
    if(!length || length>=512 || std::wstring(path).rfind(L"\\\\.\\pipe\\EntropyMusicAudit-",0)!=0){failed_=1;return;}
    pipe_=CreateFileW(path,GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
    if(pipe_==INVALID_HANDLE_VALUE){failed_=1;return;}
    event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!event_){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;failed_=1;return;}
    worker_=std::thread([this](){run();});
}
bool AuditObserver::write(const void* bytes,uint32_t count){
    OVERLAPPED pending{};pending.hEvent=event_;ResetEvent(event_);DWORD written=0;
    if(!WriteFile(pipe_,bytes,count,&written,&pending)){
        if(GetLastError()!=ERROR_IO_PENDING)return false;
        if(WaitForSingleObject(event_,100)!=WAIT_OBJECT_0){CancelIoEx(pipe_,&pending);GetOverlappedResult(pipe_,&pending,&written,TRUE);return false;}
        if(!GetOverlappedResult(pipe_,&pending,&written,FALSE))return false;
    }
    return written==count;
}
void AuditObserver::run(){
    AuditRecord r;
    for(;;){
        if(queue_.pop(r)){
            AuditHeader h;h.kind=1;h.bytes=static_cast<uint32_t>(sizeof(r.packet))+r.packet.payloadBytes;
            if(!write(&h,sizeof(h)) || !write(&r.packet,sizeof(r.packet)) || !write(r.bytes.data(),r.packet.payloadBytes)){++failed_;break;}
        }else {if(stop_)break;Sleep(1);}
    }
    AuditHeader footer;footer.kind=2;footer.overflow=queue_.overflow();footer.failedWrites=failed_;if(!write(&footer,sizeof(footer)))++failed_;
}
AuditObserver::~AuditObserver(){stop_=true;if(worker_.joinable())worker_.join();if(pipe_!=INVALID_HANDLE_VALUE)CloseHandle(pipe_);if(event_)CloseHandle(event_);}
}
