#include "Engine/Egress/LiveSinkWorkers.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace entropy;
class FakeSink final:public egress_host::Sink {
 bool fail_;HANDLE cancel_=nullptr;EntropyDacSinkConfig c_{};EntropyDacSinkStatus s_{};
public:
 explicit FakeSink(bool fail):fail_(fail){}
 void setCancelEvent(void* c) noexcept override{cancel_=HANDLE(c);}
 EntropyResult open(const EntropyDacSinkConfig& c) override{c_=c;if(fail_)return ENTROPY_ERROR_PROCESSING;s_.state=ENTROPY_DAC_SINK_OPEN;s_.buffer_frames=1024;return ENTROPY_OK;}
 EntropyResult start() noexcept override{s_.state=ENTROPY_DAC_SINK_RUNNING;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{if(WaitForSingleObject(cancel_,5)==WAIT_OBJECT_0)return ENTROPY_ERROR_INVALID_STATE;r.frames=1024;r.bytes=1024*c_.channels*(c_.output_format?4:3);s_.device_position_frames+=1024;return ENTROPY_OK;}
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{if(b.frames!=1024||b.byte_count!=1024*c_.channels*4)return ENTROPY_ERROR_PROCESSING;b.accepted_frames=b.frames;s_.submitted_frames+=b.frames;return ENTROPY_OK;}
 EntropyDacSinkStatus status() const noexcept override{return s_;}
 void stop() noexcept override{s_.state=ENTROPY_DAC_SINK_OPEN;}
 void close() noexcept override{s_.state=ENTROPY_DAC_SINK_CLOSED;}
};
int main() try {
 control::LiveSinkWorkers workers([](unsigned slot){return std::make_unique<FakeSink>(slot==1);},false);
 std::array<control::LiveDomain,2> cfg{};
 for(unsigned i=0;i<2;++i){cfg[i].enabled=true;cfg[i].channels=2;cfg[i].format=1;cfg[i].logicalId=i?"DAC_B":"DAC_A";cfg[i].endpoint=i?"fake-b":"fake-a";cfg[i].lanes[0]=i*2;cfg[i].lanes[1]=i*2+1;}
 if(!workers.prepare(cfg)||!workers.start())throw std::runtime_error("prepare/start");
 std::atomic<bool> quit=false;
 std::thread graph([&]{std::array<double,1024> pcm{};pcm.fill(.01);std::array<const double*,16> p{};p.fill(pcm.data());uint64_t frame=0;while(!quit){workers.atBlockBoundary();workers.publish(p.data(),1024,true,{1,frame,0,1024});frame+=1024;Sleep(5);}workers.graphStopped();});
 Sleep(600);const bool primaryRuns=workers.armed()&&!workers.faulted();const auto telemetry=workers.snapshot();
 workers.closePublicationGate();quit=true;graph.join();workers.stop();
 if(!primaryRuns||telemetry.asArray().size()!=2||telemetry.asArray()[0].at("state").asString()!="RUNNING"||telemetry.asArray()[1].at("state").asString()!="FAULT")throw std::runtime_error("secondary failure blocked primary");
 std::cout<<"Windows worker contracts PASS: live preparation, independent secondary failure, cancellation. Fake sinks only.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
