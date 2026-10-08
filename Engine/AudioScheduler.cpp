#include "AudioScheduler.h"
#include <windows.h>
#include <avrt.h>
#include <cmath>
namespace entropy::control {
AudioScheduler::AudioScheduler(engine::AudioGraph& g,ControlPlane& c,bool signal,OfflineSinkWorkers* sinks):graph_(g),controls_(c),signal_(signal),offlineSinks_(sinks){input_.prepare(13,256);output_.prepare(16,256);input_.clear(256);}
void AudioScheduler::start(){if(owner_.joinable())return;stopping_=false;owner_=std::thread([this]{run();});}
void AudioScheduler::stop(){stopping_=true;if(owner_.joinable())owner_.join();}
void AudioScheduler::disarm() noexcept { /* No physical sink is open while the Stage04 gate is FAIL. */ }
SchedulerTelemetry AudioScheduler::telemetry() const noexcept {return {blocks_.load(),misses_.load(),cost_.load(),maxCost_.load(),late_.load(),maxLate_.load(),faults_.load()};}
void AudioScheduler::run() noexcept {
 DWORD index=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(mmcss)AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);uint64_t sample=0;double maxCost=0,maxLate=0;auto next=std::chrono::steady_clock::now();
 while(!stopping_.load(std::memory_order_acquire)){
  auto now=std::chrono::steady_clock::now();auto lateness=std::max(0.0,std::chrono::duration<double,std::micro>(now-next).count());maxLate=std::max(maxLate,lateness);late_=lateness;maxLate_=maxLate;
  if(signal_)for(unsigned f=0;f<256;++f){double v=.001*std::sin(6.283185307179586476925286766559*1000*double(sample+f)/192000);for(unsigned c=0;c<11;++c)input_.channel(c)[f]=v;}sample+=256;
  LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
  controls_.atBlockBoundary();auto result=graph_.process(input_.readPointers(),output_.writePointers(),256);controls_.afterBlock();if(offlineSinks_&&result==ENTROPY_OK)offlineSinks_->publish(output_.readPointers(),256);
  QueryPerformanceCounter(&end);double cost=double(end.QuadPart-begin.QuadPart)*1e6/double(frequency.QuadPart);maxCost=std::max(maxCost,cost);cost_=cost;maxCost_=maxCost;if(cost>256.0/192000*1e6)misses_.fetch_add(1);if(result!=ENTROPY_OK)faults_.fetch_or(1);blocks_.fetch_add(1);
  next+=std::chrono::nanoseconds(1333333);std::this_thread::sleep_until(next); // Scheduler wait is outside processing, no sink/device is opened.
 }
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
}
