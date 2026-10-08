#include "OfflineSinkWorkers.h"
#include <windows.h>
#include <avrt.h>
#include <stdexcept>
namespace entropy::control {
void OfflineSinkWorkers::start(){
 stop();for(auto& d:domains_){egress_host::PipelineConfig config;config.channels=8;config.maximumFrames=256;config.targetFrames=4096;config.fifoFrames=32768;if(!d.pipeline.prepare(config)||!d.pipeline.arm(true,true))throw std::runtime_error("Offline egress preparation failed");d.callbacks=0;d.ppm=0;d.fill=0;d.clockState=0;}
 stopping_=false;enabled_=true;
 try{for(unsigned slot=0;slot<2;++slot)domains_[slot].owner=std::thread([this,slot]{run(slot);});}catch(...){stop();throw;}
}
void OfflineSinkWorkers::stop() noexcept {enabled_=false;stopping_=true;for(auto& d:domains_){if(d.owner.joinable())d.owner.join();d.pipeline.disarm();}}
void OfflineSinkWorkers::publish(const double* const* outputs,unsigned frames) noexcept {
 if(!enabled_.load(std::memory_order_acquire))return;
 for(unsigned slot=0;slot<2;++slot){std::array<const double*,8> lanes{};for(unsigned ch=0;ch<8;++ch)lanes[ch]=outputs[slot*8+ch];domains_[slot].pipeline.publish(lanes.data(),frames);}
}
void OfflineSinkWorkers::run(unsigned slot) noexcept {
 auto& d=domains_[slot];DWORD index=0;auto mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);if(mmcss)AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 // Waits belong to the sink scheduler, never the Graph producer or DSP calls.
 while(!stopping_&&d.pipeline.readable()<4352)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 auto next=std::chrono::steady_clock::now();uint64_t position=0;const double drift=slot?-.0001:.0001;
 while(!stopping_){d.pipeline.feedUntilTarget(256);bool ok=d.pipeline.render(256,position,10000000+uint64_t(double(position)*10000000/(192000*(1+drift))));position+=256;
  auto clock=d.pipeline.clockTelemetry();d.ppm=clock.correction_ppm;d.fill=clock.fill_frames;d.clockState=clock.state;d.callbacks.fetch_add(1);if(!ok)break;
  next+=std::chrono::nanoseconds(int64_t(1333333/(1+drift)));std::this_thread::sleep_until(next);
 }
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
ipc::Value OfflineSinkWorkers::snapshot() const {
 ipc::Value::Array list;for(unsigned slot=0;slot<2;++slot){const auto& d=domains_[slot];list.emplace_back(ipc::Value::Object{{"id",slot?"DAC B":"DAC A"},{"offlineFixture",true},{"physicalSinkOpen",false},{"state",enabled_.load()?(d.pipeline.faults()?"FAULT":"OFFLINE RUNNING"):"DISARMED"},{"clockState",d.clockState.load()},{"clockValid",d.clockState.load()==ENTROPY_EGRESS_CLOCK_LOCKED},{"ppm",d.ppm.load()},{"clockFillFrames",d.fill.load()},{"fifoFrames",d.pipeline.readable()},{"fifoCapacity",32768},{"xruns",ipc::Value::u64(d.pipeline.xruns())},{"faults",d.pipeline.faults()},{"callbacks",ipc::Value::u64(d.callbacks.load())},{"guardian","EXACT BYPASS"}});}return list;
}
}
