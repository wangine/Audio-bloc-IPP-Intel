#include "Engine/Control/LifecycleExecutor.h"
#include "Engine/AudioScheduler.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <iostream>
using namespace entropy;
using J=ipc::Value;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Seen {std::atomic<bool> fail{false};std::atomic<unsigned> opens{0},closes{0},pcm{0};};
struct Fake final:egress_host::Sink {
 Seen& seen;EntropyDacSinkConfig config{};uint64_t position=0;bool running=false;
 explicit Fake(Seen& s):seen(s){}
 EntropyResult open(const EntropyDacSinkConfig& c) override{config=c;++seen.opens;return ENTROPY_OK;}
 EntropyResult start() noexcept override{running=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{
  Sleep(5);if(seen.fail)return ENTROPY_ERROR_PROCESSING;
  r.frames=1024;r.bytes=1024*config.channels*(config.output_format?4:3);
  r.device_position_frames=position;r.device_qpc_position=10000000+position*10000000/192000;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{++seen.pcm;position+=b.frames;b.accepted_frames=b.frames;return ENTROPY_OK;}
 EntropyDacSinkStatus status() const noexcept override{EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.buffer_frames=1024;s.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;return s;}
 void stop() noexcept override{running=false;}
 void close() noexcept override{++seen.closes;}
};
template<class F>bool await(F f){auto until=GetTickCount64()+2000;while(!f()&&GetTickCount64()<until)Sleep(1);return f();}
J complete(control::LifecycleExecutor& owner,const J& admitted){J r;check(await([&]{r=owner.results(J::Array{admitted.at("commandId")}).at("results").asArray()[0];return r.at("complete").asBool();}),"command timed out");return r.at("reply");}
}
int main()try{
 engine::AudioGraph graph;check(graph.prepare(256),"graph");control::ControlPlane controls(graph);
 controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});controls.atBlockBoundary();
 Seen seen;control::LiveSinkWorkers sinks([&](unsigned){return std::make_unique<Fake>(seen);},false);
 control::AudioScheduler scheduler(graph,controls,true,nullptr,nullptr,&sinks);scheduler.start();
 std::array<control::LiveDomain,2> domains{};for(unsigned i=0;i<2;++i){auto& d=domains[i];d.enabled=true;d.channels=1;d.format=1;d.endpoint=i?"fixture-B":"fixture-A";d.logicalId=i?"DAC_B":"DAC_A";d.lanes[0]=i;}
 control::LifecycleExecutor* executor=nullptr;
 control::LifecycleExecutor owner([&](const J& q,uint64_t generation)->J{
  if(q.at("action").asString()=="arm"){
   check(!sinks.engaged(),"ARM still requires a manual DISARM after fault");
   check(sinks.prepare(domains)&&executor->activate(generation,[&]{return sinks.start();}),"arm");
   executor->stateIfCurrent(generation,control::LifecycleExecutor::State::Arming);
  }else if(q.at("action").asString()=="disarm"){sinks.stop();executor->stateIfCurrent(generation,control::LifecycleExecutor::State::Disarmed);}
  return J::Object{{"status","DONE"}};
 },[&]{sinks.closePublicationGate();},[]{},{},[&]{return sinks.faulted()&&sinks.engaged();});executor=&owner;
 check(complete(owner,owner.admit(J::Object{{"action","arm"}})).at("ok").asBool(),"initial arm");
 check(await([&]{return sinks.armed();}),"fixture never became audible");
 const auto generation=owner.generation(),blocks=scheduler.telemetry().blocks;
 seen.fail=true;
 check(await([&]{return sinks.faulted();}),"injected failure not visible");
 check(await([&]{return !sinks.engaged();}),"fault left publication engaged; automatic DISARM missing");
 check(await([&]{return seen.closes==2&&std::string(owner.stateText())=="DISARMED";}),"automatic teardown did not finish both DACs");
 check(owner.generation()>generation,"queued ARM generation not invalidated");
 const auto retained=sinks.snapshot();for(const auto& d:retained.asArray())check(d.at("faults").asUnsigned()&egress_host::Device,"original fault erased during DISARM");
 check(scheduler.telemetry().blocks>blocks,"Graph stopped with DAC fault");
 seen.fail=false;
 check(complete(owner,owner.admit(J::Object{{"action","arm"}})).at("ok").asBool(),"direct manual re-ARM refused");
 check(await([&]{return sinks.armed();}),"direct re-ARM never reached RUNNING");
 check(seen.opens==4,"unexpected automatic re-ARM or missing DAC");
 check(complete(owner,owner.admit(J::Object{{"action","disarm"}},true)).at("ok").asBool(),"final disarm");
 owner.stop();scheduler.stop();std::cout<<"PASS sink fault automatically DISARMs, retains cause, invalidates queued ARM, preserves Graph, accepts direct explicit re-ARM; no hardware\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
