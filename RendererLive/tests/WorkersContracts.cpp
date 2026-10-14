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
 bool fail_; unsigned openDelayMs_;
 HANDLE cancel_=nullptr; EntropyDacSinkConfig c_{}; EntropyDacSinkStatus s_{};
public:
 FakeSink(bool fail,unsigned delay):fail_(fail),openDelayMs_(delay){}
 void setCancelEvent(void* c) noexcept override {cancel_=HANDLE(c);}
 EntropyResult open(const EntropyDacSinkConfig& c) override {
  c_=c;
  if(openDelayMs_&&WaitForSingleObject(cancel_,openDelayMs_)==WAIT_OBJECT_0)return ENTROPY_ERROR_INVALID_STATE;
  if(fail_)return ENTROPY_ERROR_PROCESSING;
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
void scenario(unsigned healthySlot,unsigned openDelayMs) {
 control::LiveSinkWorkers workers([=](unsigned slot){return std::make_unique<FakeSink>(slot!=healthySlot,slot==healthySlot?openDelayMs:0);},false);
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
  while(!quit){workers.atBlockBoundary();workers.publish(p.data(),1024,true,{1,frame,0,1024});frame+=1024;Sleep(5);}
  workers.graphStopped();
 });
 struct Stop {
  control::LiveSinkWorkers& w;std::atomic<bool>& q;std::thread& g;
  ~Stop(){w.closePublicationGate();q=true;if(g.joinable())g.join();w.stop();}
 } cleanup{workers,quit,graph};
 // A real endpoint may take longer to open than 65536/192000 seconds.
 // Unopened endpoints have no admitted playback timeline yet; they must not
 // overflow while another endpoint is opening/failing.
 const auto until=GetTickCount64()+openDelayMs+2000;
 while(GetTickCount64()<until&&!workers.armed()&&!workers.faulted())Sleep(10);
 Sleep(100);
 const auto telemetry=workers.snapshot();const auto& rows=telemetry.asArray();
 if(!workers.armed()||workers.faulted()||rows.size()!=2||
    rows[healthySlot].at("state").asString()!="RUNNING"||
    rows[1-healthySlot].at("state").asString()!="FAULT"){
  std::cerr<<"healthySlot="<<healthySlot<<" openDelayMs="<<openDelayMs<<'\n'<<ipc::dump(telemetry)<<'\n';
  throw std::runtime_error("endpoint admission/independence failed");
 }
}
}
void runWorkersContracts() {
 scenario(0,0);
 scenario(1,0);
 scenario(0,600);
 std::cout<<"Windows worker contracts PASS: either primary, secondary failure, slow open, cancellation. Fake sinks only.\n";
}
