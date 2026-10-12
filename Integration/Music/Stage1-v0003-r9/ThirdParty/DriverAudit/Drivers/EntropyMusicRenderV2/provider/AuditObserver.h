#pragma once
#include "AuditQueue.h"
#include <thread>
#include <windows.h>
namespace entropy::music::provider {
// Footer kind2 carries overflow and failed-write counters. Missing footer
// fails acceptance. Production Provider never starts this worker.
struct AuditHeader {uint32_t magic=0x41524d45,version=1,kind=0,bytes=0;uint64_t overflow=0,failedWrites=0;};
static_assert(sizeof(AuditHeader)==32);
class AuditObserver {
    AuditQueue queue_{32,MaxPacketFrames*8};
    std::atomic<bool> stop_{false};
    std::atomic<uint64_t> failed_{0};
    HANDLE pipe_=INVALID_HANDLE_VALUE,event_=nullptr;
    std::thread worker_;
    bool write(const void*,uint32_t);
    void run();
public:
    AuditObserver();~AuditObserver();
    void observe(const EntropyIngressPacketV2& packet,const void* pcm){queue_.push(packet,pcm);}
};
}
