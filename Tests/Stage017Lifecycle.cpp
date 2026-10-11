#include "Engine/Control/LifecycleExecutor.h"
#include "Engine/Control/LiveEligibility.h"
#include "Engine/Control/DeviceService.h"
#include "Engine/AudioScheduler.h"
#include "IPC/CommandServer.h"
#include <iostream>
#include <future>
#include <algorithm>
using namespace entropy;
using J=ipc::Value;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Seen {std::atomic<unsigned> opens{0},nonzero{0};};
class SimulatedSink final:public egress_host::Sink {
 Seen& seen_;EntropyDacSinkConfig config_{};uint64_t position_=0;bool running_=false;
public:
 explicit SimulatedSink(Seen& s):seen_(s){}
 EntropyResult open(const EntropyDacSinkConfig& c) override{config_=c;++seen_.opens;return ENTROPY_OK;}
 EntropyResult start() noexcept override{running_=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{
  Sleep(5);LARGE_INTEGER now{};QueryPerformanceCounter(&now);r.frames=1024;r.bytes=r.frames*config_.channels*(config_.output_format?4:3);
  r.device_position_frames=position_;r.device_qpc_position=10000000+position_*10000000/192000;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{
  const auto* bytes=static_cast<const uint8_t*>(b.data);if(std::any_of(bytes,bytes+b.byte_count,[](auto v){return v!=0;}))++seen_.nonzero;
  b.accepted_frames=b.frames;position_+=b.frames;return ENTROPY_OK;
 }
 EntropyDacSinkStatus status() const noexcept override{EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.state=running_?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;s.buffer_frames=1024;s.device_position_frames=position_;return s;}
 void stop() noexcept override{running_=false;}
 void close() noexcept override{}
};
J waitResult(control::LifecycleExecutor& e,const J& admitted){
 const auto until=GetTickCount64()+3000;for(;;){auto v=e.results(J::Array{admitted.at("commandId")}).at("results").asArray()[0];
  if(v.at("complete").asBool())return v.at("reply");check(GetTickCount64()<until,"lifecycle completion timeout");Sleep(1);}
}
}
int main()try{
 // Regressions: retaining benchmark work in normal commands, blocking reader
 // admission, allowing a canceled prepare to start sinks, or dropping Save.
 engine::AudioGraph graph;check(graph.prepare(256),"graph prepare");control::ControlPlane controls(graph);
 controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});controls.atBlockBoundary();
 // Legacy lifecycle fixture supplies synthetic nominal timestamps. Actual
 // original-estimator calibration is covered by Stage019's dual-clock model.
 Seen seen;control::LiveSinkWorkers sinks([&](unsigned){return std::make_unique<SimulatedSink>(seen);},false);
 control::AudioScheduler scheduler(graph,controls,true,nullptr,nullptr,&sinks);scheduler.start();
 std::array<control::LiveDomain,2> domains{};auto& d=domains[0];d.enabled=true;d.channels=2;d.format=1;d.endpoint="offline-simulated";d.logicalId="DAC_A";d.lanes[0]=0;d.lanes[1]=1;
 std::promise<void> preparing,release;auto releaseFuture=release.get_future();std::atomic<bool> slow{true};unsigned saved=0;
 control::LifecycleExecutor* executor=nullptr;
 control::LifecycleExecutor owner([&](const J& request,uint64_t generation)->J{
  const auto action=request.at("action").asString();
  if(action=="arm"){
   if(slow.exchange(false)){preparing.set_value();releaseFuture.wait();}
   check(sinks.prepare(domains),"prepared sink configuration");
   const bool activated=executor->activate(generation,[&]{return sinks.start();});
   if(!activated)throw std::runtime_error("ARM canceled");return J::Object{{"status","ARMING"}};
  }
  if(action=="disarm"){const auto start=std::chrono::steady_clock::now();sinks.stop();return J::Object{{"teardownUs",std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()}};}
  if(action=="save-profile"){++saved;return J::Object{{"status","SAVED"}};}
  controls.execute(request);return J::Object{{"status","ACCEPTED_AT_CONTROL"}};
 },[&]{sinks.closePublicationGate();},[]{});executor=&owner;
 auto arm=owner.admit(J::Object{{"action","arm"}});preparing.get_future().wait();
 auto save=owner.admit(J::Object{{"action","save-profile"}});
 auto disarm=owner.admit(J::Object{{"action","disarm"}},true);
 check(disarm.at("admissionUs").asNumber()<50000&&disarm.at("gateUs").asNumber()<50000,"DISARM admission/gate exceeded 50ms");
 check(!sinks.engaged(),"publication gate not immediately closed");
 check(!owner.results(J::Array{arm.at("commandId")}).at("results").asArray()[0].at("complete").asBool(),"slow preparation finished before DISARM admission");
 release.set_value();check(!waitResult(owner,arm).at("ok").asBool(),"canceled ARM completed successfully");
 check(waitResult(owner,disarm).at("ok").asBool(),"DISARM completion");
 check(waitResult(owner,save).at("ok").asBool()&&saved==1,"DISARM discarded confirmed profile request");check(seen.opens==0,"late preparation opened sink");
 const auto beforeArm=scheduler.telemetry().blocks;
 auto next=owner.admit(J::Object{{"action","arm"}});check(waitResult(owner,next).at("ok").asBool(),"explicit new ARM");
 auto until=GetTickCount64()+2000;while(!sinks.armed()&&GetTickCount64()<until)Sleep(1);check(sinks.armed(),"simulated sink did not reach RUNNING");
 auto cinema=owner.admit(J::Object{{"action","program"},{"program",1}});check(waitResult(owner,cinema).at("ok").asBool(),"CINEMA command");Sleep(65);
 auto music=owner.admit(J::Object{{"action","program"},{"program",0}});check(waitResult(owner,music).at("ok").asBool(),"MUSIC command");Sleep(65);
 auto stopped=owner.admit(J::Object{{"action","disarm"}},true);auto stoppedResult=waitResult(owner,stopped);
 check(stoppedResult.at("ok").asBool()&&!sinks.armed()&&seen.nonzero>0,"simulated ARM/program/DISARM did not render active PCM");
 const auto afterDisarm=scheduler.telemetry().blocks;Sleep(10);check(afterDisarm>beforeArm&&scheduler.telemetry().blocks>afterDisarm,"Graph did not continue through ARM/DISARM");
 check(control::diagnosticBenchmarkCalls()==0,"normal commands invoked benchmark");owner.stop();scheduler.stop();
 // Native safety is still applied, including duplicate lanes and bad PCM width.
 auto invalid=domains;invalid[0].lanes[1]=invalid[0].lanes[0];check(!sinks.prepare(invalid),"duplicate routing accepted");invalid=domains;invalid[0].format=2;check(!sinks.prepare(invalid),"invalid PCM format accepted");
 control::DeviceService devices;try{devices.configure(J::Object{{"domains",J::Array{J::Object{{"enabled",true},{"logicalId","DAC_A"},{"approvalRevision",J::u64(1)},{"endpoint","invalid"},{"rate",48000},{"channels",2},{"format",1},{"lanes",J::Array{0,1}}}}}});throw std::runtime_error("Invalid rate accepted");}catch(const std::invalid_argument&){}
 // A gate-closing command is still accepted when the ordinary queue is full;
 // failure must complete teardown, and an expired ID cannot poison a batch.
 std::promise<void> blocked,unblock;auto unblocked=unblock.get_future();unsigned teardowns=0;
 control::LifecycleExecutor failureOwner([&](const J& request,uint64_t)->J{
  const auto action=request.at("action").asString();
  if(action=="hold"){blocked.set_value();unblocked.wait();}
  if(action=="load-profile")throw std::invalid_argument("Missing offline fixture profile");
  if(action=="disarm")++teardowns;
  return J::Object{{"status","DONE"}};
 },[]{},[]{});
 failureOwner.admit(J::Object{{"action","hold"}});blocked.get_future().wait();
 for(unsigned n=0;n<256;++n)failureOwner.admit(J::Object{{"action","save-profile"}});
 auto failedLoad=failureOwner.admit(J::Object{{"action","load-profile"}},true);unblock.set_value();
 check(!waitResult(failureOwner,failedLoad).at("ok").asBool()&&teardowns==1&&std::string(failureOwner.stateText())=="DISARMED","gate-closing failure stuck in STOPPING");
 auto batch=failureOwner.results(J::Array{J::u64(999999),failedLoad.at("commandId")}).at("results").asArray();
 check(batch[0].at("complete").asBool()&&batch[0].at("reply").at("status").asString()=="RESULT_EXPIRED"&&batch[1].at("complete").asBool(),"expired identity poisoned batch");failureOwner.stop();
 // A fatal fault discovered during source teardown must invalidate an ARM
 // already admitted while that teardown was blocked.
 std::promise<void> fatalEntered,fatalRelease;auto fatalReleased=fatalRelease.get_future();
 std::atomic<bool> fatalTick{true};unsigned activated=0;control::LifecycleExecutor* fatalExecutor=nullptr;
 control::LifecycleExecutor fatalOwner([&](const J& q,uint64_t g)->J{
  if(q.at("action").asString()=="arm")check(fatalExecutor->activate(g,[&]{++activated;return true;}),"fatal ARM canceled");
  return J::Object{{"status","DONE"}};
 },[]{},[&]{if(fatalExecutor&&fatalTick.exchange(false)){fatalEntered.set_value();fatalReleased.wait();fatalExecutor->disarmAfterFault();}});
 fatalExecutor=&fatalOwner;fatalEntered.get_future().wait();
 auto queuedFatalArm=fatalOwner.admit(J::Object{{"action","arm"}});fatalRelease.set_value();
 check(!waitResult(fatalOwner,queuedFatalArm).at("ok").asBool()&&activated==0,"late fatal teardown allowed previously queued ARM");
 check(waitResult(fatalOwner,fatalOwner.admit(J::Object{{"action","arm"}})).at("ok").asBool()&&activated==1,"fresh post-fault ARM rejected");fatalOwner.stop();
 std::cout<<ipc::dump(J::Object{{"PASS",true},{"slowPreparationDisarmAdmissionUs",disarm.at("admissionUs")},{"slowPreparationGateUs",disarm.at("gateUs")},{"runningDisarmAdmissionUs",stopped.at("admissionUs")},{"runningGateUs",stopped.at("gateUs")},{"simulatedTeardownUs",stoppedResult.at("data").at("teardownUs")},{"benchmarkCalls",J::u64(control::diagnosticBenchmarkCalls())},{"activePcmBlocks",unsigned(seen.nonzero)},{"lateSinkOpens",0}})<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
