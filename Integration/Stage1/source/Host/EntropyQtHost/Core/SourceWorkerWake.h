#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
namespace entropy::host {
// Local, single SRC waiter. Auto-reset event retains a notify before the wait;
// the revision detects coalesced notifications. Neither is exposed in an ABI.
// Owner stops/notifies/joins workers before destroying this object.
class SourceWorkerWake final {
 std::atomic<std::uint64_t> revision_{0};
 HANDLE event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
public:
 SourceWorkerWake() noexcept=default;
 ~SourceWorkerWake(){if(event_)CloseHandle(event_);}
 SourceWorkerWake(const SourceWorkerWake&)=delete;
 SourceWorkerWake& operator=(const SourceWorkerWake&)=delete;
 bool valid() const noexcept{return event_!=nullptr;}
 std::uint64_t snapshot() const noexcept{return revision_.load(std::memory_order_acquire);}
 void notify() noexcept{revision_.fetch_add(1,std::memory_order_release);if(event_)SetEvent(event_);}
 bool waitForChange(std::uint64_t observed) noexcept{
  if(snapshot()!=observed)return true;
  return event_&&WaitForSingleObject(event_,INFINITE)==WAIT_OBJECT_0;
 }
};
}
