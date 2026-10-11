#include "MusicCinemaFixture.h"
#include "Engine/Control/LifecycleExecutor.h"
#include "Engine/AudioScheduler.h"
#include "Engine/Control/SourceWaitPolicy.h"
using J=ipc::Value;
namespace {
struct Seen {std::atomic<unsigned> opens{0},closes{0},submitted{0};std::atomic<bool> holdStop{false},stopping{false},releaseStop{false};};
struct Fake:egress_host::Sink {
 Seen& seen;EntropyDacSinkConfig config{};uint64_t position=0;bool running=false;
 explicit Fake(Seen& s):seen(s){}
 EntropyResult open(const EntropyDacSinkConfig& c)override{config=c;++seen.opens;return ENTROPY_OK;}
 EntropyResult start()noexcept override{running=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r)noexcept override{
  Sleep(5);r.frames=1024;r.bytes=1024*config.channels*(config.output_format?4:3);
  r.device_position_frames=position;r.device_qpc_position=10000000+position*10000000/192000;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b)noexcept override{++seen.submitted;position+=b.frames;b.accepted_frames=b.frames;return ENTROPY_OK;}
 EntropyDacSinkStatus status()const noexcept override{EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.buffer_frames=1024;s.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;return s;}
 void stop()noexcept override{running=false;if(seen.holdStop){seen.stopping=true;while(!seen.releaseStop)Sleep(1);}}
 void close()noexcept override{++seen.closes;}
};
template<class F>bool await(F f){const auto until=GetTickCount64()+3000;while(!f()&&GetTickCount64()<until)Sleep(1);return f();}
J complete(control::LifecycleExecutor& owner,const J& accepted){J r;check(await([&]{r=owner.results(J::Array{accepted.at("commandId")}).at("results").asArray()[0];return r.at("complete").asBool();}),"command timeout");return r.at("reply");}
}
int main(int argc,char** argv)try {
 if(argc>1){
  Seen seen;control::LiveSinkWorkers sinks([&](unsigned){return std::make_unique<Fake>(seen);},false);
  std::array<control::LiveDomain,2> domains{};auto& d=domains[0];d.enabled=true;d.channels=1;d.format=1;d.endpoint="fixture-A";d.logicalId="DAC_A";
  std::array<double,1024> audio{};std::array<const double*,16> output{};output.fill(audio.data());
  check(sinks.prepare(domains)&&sinks.start(),"fault fixture start");sinks.publish(output.data(),1024,true);
  control::SourceWaitPolicy policy(sinks);policy.requestArm();
  if(std::string(argv[1])=="rearm-absent"){
   sinks.failAll(egress_host::Input);sinks.stop();policy.cancel();
   check(sinks.prepare(domains),"explicit absent-input ARM preparation");
   policy.requestArm(true);
   const auto result=policy.service(true,1,true,[&]{return sinks.prepare(domains)&&sinks.start();});
   check(result==control::SourceWaitPolicy::Result::Resumed&&policy.requested(),"previous fatal session canceled explicit absent-input ARM");
  }else if(std::string(argv[1])=="provenance"){
   sinks.failAll(egress_host::Input);sinks.waitForInput();
   check(!sinks.sourceWaitFault(),"transport loss relabeled fatal Graph Input as source wait");
   check(sinks.prepare(domains)&&sinks.start(),"reverse fixture start");sinks.publish(output.data(),1024,true);
   sinks.waitForInput();sinks.failAll(egress_host::Input);
   check(!sinks.sourceWaitFault(),"Graph Input after source hold lost fatal provenance");
  }else{
   check(await([&]{return seen.opens.load()>0;}),"fixture open");seen.holdStop=true;sinks.waitForInput();
   std::thread service([&]{policy.service(false,1,true,[]{return false;});});
   const bool stopped=await([&]{return seen.stopping.load();});
   // The sink may enter stop directly after waitForInput(), before policy service.
   // Inject only after the lifecycle owner enters its blocked teardown.
   const bool retiring=stopped&&await([&]{return !sinks.engaged();});
   if(retiring)sinks.failAll(egress_host::Clock);
   seen.releaseStop=true;service.join();
   check(stopped,"teardown barrier not reached");
   check(retiring,"lifecycle teardown barrier not reached");
   check(!policy.requested()&&!policy.muted(),"fatal fault during source teardown retained ARM");
   unsigned attempts=0;policy.service(true,2,true,[&]{++attempts;return true;});
   check(!attempts,"fatal stopped session auto-resumed");
  }
  sinks.stop();std::cout<<"PASS fatal source-wait interaction\n";return 0;
 }
 auto source=std::make_unique<Source>(4,81,0,.00001);
 input::ProgramIngress ingress;check(ingress.start(source->endpoint,input::defaultEndpoint(input::Domain::Cinema)+L".absent",false),"input");
 input::ProgramSelection selection;selection.quiescent(0);
 engine::AudioGraph graph;check(graph.prepare(1024),"graph");control::ControlPlane controls(graph);
 controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});controls.atBlockBoundary();
 Seen seen;control::LiveSinkWorkers sinks([&](unsigned){return std::make_unique<Fake>(seen);},false);
 control::SourceWaitPolicy sourceWait(sinks);
 control::AudioScheduler scheduler(graph,controls,ingress,selection,&sinks);scheduler.start();
 std::array<control::LiveDomain,2> domains{};auto& d=domains[0];d.enabled=true;d.channels=1;d.format=1;d.endpoint="fixture-A";d.logicalId="DAC_A";d.lanes[0]=0;
 control::LifecycleExecutor* executor=nullptr;
 control::LifecycleExecutor owner([&](const J& q,uint64_t g)->J{
  if(q.at("action").asString()=="arm"){
   check(sinks.prepare(domains)&&executor->activate(g,[&]{const bool started=sinks.start();if(started)sourceWait.requestArm();return started;}),"arm");
   executor->stateIfCurrent(g,control::LifecycleExecutor::State::Arming);
  }else if(q.at("action").asString()=="disarm"){sourceWait.cancel();sinks.stop();executor->stateIfCurrent(g,control::LifecycleExecutor::State::Disarmed);}
  return J::Object{{"status","DONE"}};
 },[&]{sourceWait.cancel();sinks.closePublicationGate();},[&]{
  if(!executor)return;
  const auto g=executor->generation();
  const auto source=ingress.telemetry(input::Domain::Music);
  sourceWait.service(source.state==input::State::Receiving,source.lastBlockQpc,true,[&]{return sinks.prepare(domains)&&executor->activate(g,[&]{return sourceWait.requested()&&sinks.start();});});
  if(sourceWait.muted())executor->stateIfCurrent(g,control::LifecycleExecutor::State::WaitingInput);
 },{},[&]{return sinks.faulted()&&sinks.engaged()&&!sinks.sourceWaitFault();});executor=&owner;
 check(complete(owner,owner.admit(J::Object{{"action","arm"}})).at("ok").asBool(),"initial arm");
 check(await([&]{return sinks.armed();}),"initial playback");
 source.reset();
 check(await([&]{return std::string(owner.stateText())=="WAITING_INPUT";}),"source loss must retain user ARM as WAITING_INPUT, not DISARM");
 const auto blocks=scheduler.telemetry().blocks;
 const auto submitted=seen.submitted.load();
 Sleep(350);
 check(scheduler.telemetry().blocks==blocks,"waiting fabricated Graph PCM");
 check(seen.submitted==submitted,"waiting submitted fabricated sink PCM");
 source=std::make_unique<Source>(4,81,1,.00001);
 check(await([&]{return sinks.armed()&&scheduler.telemetry().blocks>blocks+20;}),"validated source did not resume same Core");
 source.reset();
 check(await([&]{return std::string(owner.stateText())=="WAITING_INPUT";}),"second source wait");
 check(complete(owner,owner.admit(J::Object{{"action","disarm"}},true)).at("ok").asBool(),"explicit disarm");
 const auto opened=seen.opens.load();source=std::make_unique<Source>(4,81,2,.00001);Sleep(350);
 check(!sinks.engaged()&&seen.opens==opened,"explicit DISARM did not cancel source resumption");
 owner.stop();scheduler.stop();ingress.stop();
 std::cout<<"PASS source wait retains user ARM, no false Graph/sink PCM, validated UUID resume, explicit DISARM cancels; no hardware\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
