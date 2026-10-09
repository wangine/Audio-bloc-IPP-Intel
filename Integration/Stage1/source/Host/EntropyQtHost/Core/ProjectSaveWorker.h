#pragma once
#include "ProjectPersistence.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <type_traits>

namespace entropy::host {
struct ProjectSaveRequest { std::uint64_t sequence=0; std::array<wchar_t,1024> path{}; };
struct ProjectSaveOutcome { std::uint64_t sequence=0; bool ok=false; std::array<wchar_t,1024> path{}; std::array<char,512> detail{}; };
// One producer, one persistent nonRT consumer. A single reservation includes
// the unread outcome; neither submitting nor polling launches work or allocates.
class ProjectSaveWorker final {
public:
 using Capture=std::function<bool(EntropyProjectDocument&)>;
 using Write=std::function<ProjectIoResult(const std::filesystem::path&,const EntropyProjectDocument&)>;
 ProjectSaveWorker(Capture capture,Write write=saveProjectAtomic,
     std::chrono::milliseconds timeout=std::chrono::seconds(10)):
     capture_(std::move(capture)),write_(std::move(write)),timeout_(timeout),thread_([this]{run();}){}
 ~ProjectSaveWorker(){stop();}
 ProjectSaveWorker(const ProjectSaveWorker&)=delete;
 ProjectSaveWorker& operator=(const ProjectSaveWorker&)=delete;
 bool submit(const ProjectSaveRequest& request) noexcept {
   if(request.sequence==0||request.path[0]==0||stopping_.load(std::memory_order_acquire)||busy_.load(std::memory_order_acquire))return false;
   request_=request;request_.path.back()=0;producerReady_.store(0,std::memory_order_relaxed);
   observerReady_.store(0,std::memory_order_relaxed);
   sequence_.store(request.sequence,std::memory_order_release);
   freeze_.store(true,std::memory_order_release);busy_.store(true,std::memory_order_release);
   pending_.store(true,std::memory_order_release);return true;
 }
 bool poll(ProjectSaveOutcome& outcome) noexcept {
   if(!complete_.load(std::memory_order_acquire))return false;
   outcome=outcome_;complete_.store(false,std::memory_order_relaxed);
   busy_.store(false,std::memory_order_release);return true;
 }
 bool busy()const noexcept{return busy_.load(std::memory_order_acquire);}
 bool frozen()const noexcept{return freeze_.load(std::memory_order_acquire);}
 std::uint64_t sequence()const noexcept{return sequence_.load(std::memory_order_acquire);}
 bool observerReady()const noexcept{return observerReady_.load(std::memory_order_acquire)==sequence();}
 void observerGated(std::uint64_t sequence)noexcept{observerReady_.store(sequence,std::memory_order_release);}
 void producerDrained()noexcept{producerReady_.store(sequence(),std::memory_order_release);}
 void stop()noexcept{stopping_.store(true,std::memory_order_release);if(thread_.joinable())thread_.join();}
private:
 void run()noexcept{
   while(!stopping_.load(std::memory_order_acquire)){
     if(!pending_.exchange(false,std::memory_order_acquire)){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
     ProjectSaveOutcome result{};result.sequence=request_.sequence;result.path=request_.path;
     const auto deadline=std::chrono::steady_clock::now()+timeout_;
     bool ready=false;
     while(!(ready=producerReady_.load(std::memory_order_acquire)==result.sequence&&observerReady_.load(std::memory_order_acquire)==result.sequence)&&
       !stopping_.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<deadline)
       std::this_thread::sleep_for(std::chrono::milliseconds(1));
     ProjectIoResult io{false,"Save snapshot drain timed out"};
     try {
       EntropyProjectDocument project;
       const bool captured=ready&&!stopping_.load(std::memory_order_acquire)&&capture_(project);
       // Snapshot ownership ends before any JSON encoding or disk operation.
       freeze_.store(false,std::memory_order_release);
       if(captured)io=write_(std::filesystem::path(result.path.data()),project);
       else if(ready)io={false,"Could not capture required or present optional module state through ABI"};
     }catch(...){freeze_.store(false,std::memory_order_release);io={false,"Save worker failed"};}
     result.ok=io.ok;
     std::strncpy(result.detail.data(),io.detail.c_str(),result.detail.size()-1);
     outcome_=result;complete_.store(true,std::memory_order_release);
   }
   freeze_.store(false,std::memory_order_release);
 }
 static_assert(std::atomic<bool>::is_always_lock_free);
 static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
 static_assert(std::is_trivially_copyable_v<ProjectSaveRequest>);
 static_assert(std::is_trivially_copyable_v<ProjectSaveOutcome>);
 Capture capture_;Write write_;std::chrono::milliseconds timeout_;
 ProjectSaveRequest request_{};ProjectSaveOutcome outcome_{};
 std::atomic<bool> busy_{false},freeze_{false},pending_{false},complete_{false},stopping_{false};
 std::atomic<std::uint64_t> sequence_{0},producerReady_{0},observerReady_{0};
 std::thread thread_;
};
}
