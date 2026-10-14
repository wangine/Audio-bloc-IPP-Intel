#include "Engine/Egress/LiveSinkWorkers.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace entropy;
namespace {
class FakeSink final : public egress_host::Sink {
 bool fail_,deferred_;
 std::atomic<bool>& opening_;std::atomic<bool>& releaseOpen_;
 HANDLE cancel_=nullptr;EntropyDacSinkConfig c_{};EntropyDacSinkStatus s_{};
public:
 FakeSink(bool fail,bool deferred,std::atomic<bool>& opening,std::atomic<bool>& releaseOpen)
  :fail_(fail),deferred_(deferred),opening_(opening),releaseOpen_(releaseOpen){}
 void setCancelEvent(void* c) noexcept override {cancel_=HANDLE(c);}
 EntropyResult open(const EntropyDacSinkConfig& c) override {
  c_=c;if(fail_)return ENTROPY_ERROR_PROCESSING;
  if(deferred_){
   opening_.store(true,std::memory_order_release);
   while(!releaseOpen_.load(std::memory_order_acquire))
    if(WaitForSingleObject(cancel_,1)==WAIT_OBJECT_0)return ENTROPY_ERROR_INVALID_STATE;
  }
  s_.state=ENTROPY_DAC_SINK_OPEN;s_.buffer_frames=1024;return ENTROPY_OK;
 }
 EntropyResult start() noexcept override {s_.state=ENTROPY_DAC_SINK_RUNNING;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override {
  if(WaitForSingleObject(cancel_,5)==WAIT_OBJECT_0)return ENTROPY_ERROR_INVALID_STATE;
  r.frames=1024;r.bytes=1024*c_.channels*(c_.output_format?4:3);
  s_.device_position_frames+=1024;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override {
  if(b.frames!=1024||b.byte_count!=1024*c_.channels*4)return ENTROPY_ERROR_PROCESSING;
  b.accepted_frames=b.frames;s_.submitted_frames+=b.frames;return ENTROPY_OK;
 }
 EntropyDacSinkStatus status() const noexcept override {return s_;}
 void stop() noexcept override {s_.state=ENTROPY_DAC_SINK_OPEN;}
 void close() noexcept override {s_.state=ENTROPY_DAC_SINK_CLOSED;}
};
void scenario(unsigned healthySlot,bool deferredOpen) {
 std::atomic<bool> opening=false,releaseOpen=false;
 control::LiveSinkWorkers workers([&](unsigned slot){return std::make_unique<FakeSink>(slot!=healthySlot,deferredOpen,opening,releaseOpen);},false);
 std::array<control::LiveDomain,2> cfg{};
 for(unsigned i=0;i<2;++i){
  cfg[i].enabled=true;cfg[i].channels=2;cfg[i].format=1;
  cfg[i].logicalId=i?"DAC_B":"DAC_A";cfg[i].endpoint=i?"fake-b":"fake-a";
  cfg[i].lanes[0]=i*2;cfg[i].lanes[1]=i*2+1;
 }
 if(!workers.prepare(cfg)||!workers.start())throw std::runtime_error("prepare/start");
 std::atomic<bool> quit=false;
 std::thread graph([&]{
  std::array<double,1024> pcm{};pcm.fill(.01);std::array<const double*,16> p{};p.fill(pcm.data());uint64_t frame=0;
  auto publish=[&]{workers.atBlockBoundary();workers.publish(p.data(),1024,true,{1,frame,0,1024});frame+=1024;};
  if(deferredOpen){
   const auto deadline=GetTickCount64()+2000;
   while(!opening.load(std::memory_order_acquire)&&!quit&&GetTickCount64()<deadline)Sleep(1);
   // Force more than one queue capacity to arrive BEFORE open() returns.
   // This is a startup-admission check, not a real-time throughput model.
   // No dependency on Windows' Sleep(5) timer granularity.
   for(unsigned n=0;n<128&&!quit;++n)publish();
   releaseOpen.store(true,std::memory_order_release);
  }
  while(!quit){publish();Sleep(5);}
  workers.graphStopped();
 });
 struct Stop {
  control::LiveSinkWorkers& w;std::atomic<bool>& q;std::thread& g;
  ~Stop(){w.closePublicationGate();q=true;if(g.joinable())g.join();w.stop();}
 } cleanup{workers,quit,graph};
 const auto until=GetTickCount64()+3500;
 while(GetTickCount64()<until&&!workers.armed()&&!workers.faulted())Sleep(10);
 Sleep(100);
 const auto telemetry=workers.snapshot();const auto& rows=telemetry.asArray();
 if(!workers.armed()||workers.faulted()||rows.size()!=2||
    rows[healthySlot].at("state").asString()!="RUNNING"||
    rows[1-healthySlot].at("state").asString()!="FAULT"){
  std::cerr<<"healthySlot="<<healthySlot<<" deferredOpen="<<deferredOpen<<'\n'<<ipc::dump(telemetry)<<'\n';
  throw std::runtime_error("endpoint admission/independence failed");
 }
}
}
void runWorkersContracts() {
 scenario(0,false);
 scenario(1,false);
 scenario(0,true);
 std::cout<<"Windows worker contracts PASS: either primary, secondary failure, deferred admission, cancellation. Fake sinks only.\n";
}
