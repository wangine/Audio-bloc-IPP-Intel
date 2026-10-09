#include "AudioScheduler.h"
#include "Control/LiveEligibility.h"
#include "HighResolutionWait.h"
#include <windows.h>
#include <avrt.h>
#include <cmath>
namespace entropy::control {
AudioScheduler::AudioScheduler(engine::AudioGraph& g,ControlPlane& c,bool signal,OfflineSinkWorkers* sinks,input::Stage2Ingress* stage2,LiveSinkWorkers* live):graph_(g),controls_(c),signal_(signal),offlineSinks_(sinks),stage2_(stage2),liveSinks_(live),frames_(stage2?1024:256){input_.prepare(13,frames_);output_.prepare(16,frames_);input_.clear(frames_);}
void AudioScheduler::start(){if(owner_.joinable())return;if(stage2_)stage2_->rebaseCalendar();stopping_=false;owner_=std::thread([this]{run();});}
void AudioScheduler::stop(){stopping_=true;if(owner_.joinable())owner_.join();}
void AudioScheduler::disarm() noexcept {if(liveSinks_)liveSinks_->stop();}
SchedulerTelemetry AudioScheduler::telemetry() const noexcept {return {blocks_.load(),misses_.load(),cost_.load(),maxCost_.load(),late_.load(),maxLate_.load(),faults_.load()};}
void AudioScheduler::run() noexcept {
 HighResolutionWait wait;
 if(!wait.valid()){faults_.fetch_or(2);if(liveSinks_)liveSinks_->failAll(egress_host::Device);return;}
 DWORD index=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(mmcss)AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);uint64_t sample=0;double maxCost=0,maxLate=0;auto next=std::chrono::steady_clock::now();
 while(!stopping_.load(std::memory_order_acquire)){
  auto now=std::chrono::steady_clock::now();auto lateness=std::max(0.0,std::chrono::duration<double,std::micro>(now-next).count());maxLate=std::max(maxLate,lateness);late_=lateness;maxLate_=maxLate;
  if(signal_)for(unsigned f=0;f<frames_;++f){double v=.001*std::sin(6.283185307179586476925286766559*1000*double(sample+f)/192000);for(unsigned c=0;c<11;++c)input_.channel(c)[f]=v;}sample+=frames_;
  LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
  if(stage2_&&!stage2_->tryRead(input_.writePointers())){
   // Transport starvation never inserts invented samples into DSP histories.
   next=std::chrono::steady_clock::now();wait.forDuration(std::chrono::microseconds(100));continue;
  }
  controls_.atBlockBoundary();auto result=graph_.process(input_.readPointers(),output_.writePointers(),frames_);controls_.afterBlock();if(offlineSinks_&&result==ENTROPY_OK)offlineSinks_->publish(output_.readPointers(),frames_);
  if(liveSinks_&&liveSinks_->engaged()){if(result==ENTROPY_OK)liveSinks_->publish(output_.readPointers(),frames_,liveHeadroomReady(graph_,false));else liveSinks_->failAll(egress_host::Input);}
  QueryPerformanceCounter(&end);double cost=double(end.QuadPart-begin.QuadPart)*1e6/double(frequency.QuadPart);maxCost=std::max(maxCost,cost);cost_=cost;maxCost_=maxCost;if(cost>double(frames_)/192000*1e6)misses_.fetch_add(1);if(result!=ENTROPY_OK)faults_.fetch_or(1);blocks_.fetch_add(1);
  if(stage2_)stage2_->outputAtTarget(liveSinks_&&liveSinks_->recoveryAtTarget());
  next+=std::chrono::nanoseconds(uint64_t(frames_)*1000000000/192000);
  // Real transport already supplies the source clock; do not pace it a second time.
  if(!stage2_)wait.until(next); // Outside processing, no sink/device is opened.
 }
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
}
