#include "AudioScheduler.h"
#include "Control/LiveEligibility.h"
#include "Control/GraphControlAccess.h"
#include "HighResolutionWait.h"
#include <windows.h>
#include <avrt.h>
#include <cmath>
namespace entropy::control {
AudioScheduler::AudioScheduler(engine::AudioGraph& g,ControlPlane& c,bool signal,OfflineSinkWorkers* sinks,input::Stage2Ingress* stage2,LiveSinkWorkers* live):graph_(g),controls_(c),signal_(signal),offlineSinks_(sinks),stage2_(stage2),liveSinks_(live),frames_(stage2?1024:256){input_.prepare(13,frames_);output_.prepare(16,frames_);input_.clear(frames_);}
void AudioScheduler::start(){if(owner_.joinable())return;if(stage2_){if(blocks_.load())controls_.crossover().resetQuiescent();stage2_->restartSubscription();}stopping_=false;owner_=std::thread([this]{run();});}
AudioScheduler::AudioScheduler(engine::AudioGraph& g,ControlPlane& c,input::ProgramIngress& in,input::ProgramSelection& selection,LiveSinkWorkers* live):AudioScheduler(g,c,false,nullptr,nullptr,live){programInput_=&in;selection_=&selection;frames_=1024;input_.prepare(13,frames_);output_.prepare(16,frames_);input_.clear(frames_);}
void AudioScheduler::stop(){stopping_=true;if(owner_.joinable())owner_.join();if(liveSinks_)liveSinks_->graphStopped();}
void AudioScheduler::disarm() noexcept {if(liveSinks_)liveSinks_->stop();}
SchedulerTelemetry AudioScheduler::telemetry() const noexcept {return {blocks_.load(),misses_.load(),cost_.load(),maxCost_.load(),late_.load(),maxLate_.load(),faults_.load(),maximumInputIntervalUs_.load()};}
void AudioScheduler::run() noexcept {
 HighResolutionWait wait;
 if(!wait.valid()){faults_.fetch_or(2);if(liveSinks_)liveSinks_->failAll(egress_host::Device);return;}
 DWORD index=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(mmcss)AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);uint64_t sample=0,graphFrame=0,graphEpoch=1,lastEpoch=0,lastGap=0,lastSequence=0,musicToken=0,cinemaToken=0,previousInputQpc=0;double maxCost=0,maxLate=0;auto next=std::chrono::steady_clock::now();
 while(!stopping_.load(std::memory_order_acquire)){
  auto now=std::chrono::steady_clock::now();auto lateness=std::max(0.0,std::chrono::duration<double,std::micro>(now-next).count());maxLate=std::max(maxLate,lateness);late_=lateness;maxLate_=maxLate;
  if(signal_)for(unsigned f=0;f<frames_;++f){double v=.001*std::sin(6.283185307179586476925286766559*1000*double(sample+f)/192000);for(unsigned c=0;c<11;++c)input_.channel(c)[f]=v;}sample+=frames_;
  LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
  controls_.atBlockBoundary();if(liveSinks_)liveSinks_->atBlockBoundary();
  if(programInput_){input::GraphInputContext context;auto needed=selection_->requiredDomains();
   if(!programInput_->tryRead(input_.writePointers(),needed,context)){controls_.afterBlock();if(programInput_->faulted(needed)&&liveSinks_&&liveSinks_->engaged())liveSinks_->failAll(egress_host::Input);next=std::chrono::steady_clock::now();wait.forDuration(std::chrono::microseconds(100));continue;}
   // Source histories have their own epochs. The continuous Graph-to-DAC
   // timeline does not restart when a source format/generation or program changes.
   if(needed&1)musicToken=context.music.token;if(needed&2)cinemaToken=context.cinema.token;
   engine::GraphControlAccess::crossover(graph_)->setProgramInputEpochs(musicToken,musicToken,cinemaToken,0xffffu);
  }
  if(stage2_&&!stage2_->tryRead(input_.writePointers())){
   // Transport starvation never inserts invented samples into DSP histories.
   controls_.afterBlock();
   if(stage2_->telemetry().state==input::State::Fault&&liveSinks_&&liveSinks_->engaged())liveSinks_->failAll(egress_host::Input);
   next=std::chrono::steady_clock::now();wait.forDuration(std::chrono::microseconds(100));continue;
  }
  if(stage2_){const auto h=stage2_->lastBlock();const auto t=stage2_->telemetry();if(lastEpoch!=h.stream_epoch||t.gaps!=lastGap||(lastSequence&&h.block_sequence!=lastSequence+1)){++graphEpoch;lastEpoch=h.stream_epoch;lastGap=t.gaps;}lastSequence=h.block_sequence;engine::GraphControlAccess::crossover(graph_)->setProgramInputEpochs(h.music_session_revision,h.source_generation[0],h.source_generation[2],0xffffu);}
  if(programInput_){if(previousInputQpc){const double interval=double(uint64_t(begin.QuadPart)-previousInputQpc)*1e6/double(frequency.QuadPart);maximumInputIntervalUs_=std::max(maximumInputIntervalUs_.load(std::memory_order_relaxed),interval);}previousInputQpc=uint64_t(begin.QuadPart);}
  auto result=graph_.process(input_.readPointers(),output_.writePointers(),frames_);controls_.afterBlock();if(offlineSinks_&&result==ENTROPY_OK)offlineSinks_->publish(output_.readPointers(),frames_);
  if(selection_)selection_->afterBlock(controls_.crossover().settled(),engine::GraphControlAccess::crossover(graph_)->programBlockState().program);
  if(liveSinks_&&liveSinks_->engaged()){if(result==ENTROPY_OK)liveSinks_->publish(output_.readPointers(),frames_,liveHeadroomReady(graph_,false),egress_host::GraphInterval{graphEpoch,graphFrame,uint64_t(static_cast<long double>(begin.QuadPart)*10000000/frequency.QuadPart),frames_});else liveSinks_->failAll(egress_host::Input);}
  graphFrame+=frames_;QueryPerformanceCounter(&end);double cost=double(end.QuadPart-begin.QuadPart)*1e6/double(frequency.QuadPart);maxCost=std::max(maxCost,cost);cost_=cost;maxCost_=maxCost;if(cost>double(frames_)/192000*1e6)misses_.fetch_add(1);if(result!=ENTROPY_OK)faults_.fetch_or(1);blocks_.fetch_add(1);
  next+=std::chrono::nanoseconds(uint64_t(frames_)*1000000000/192000);
  // Real transport already supplies the source clock; do not pace it a second time.
  if(!stage2_&&!programInput_)wait.until(next); // Outside processing, no sink/device is opened.
 }
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
}
