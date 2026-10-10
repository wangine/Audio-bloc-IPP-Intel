#include "CoreRuntime.h"
#include "ControlPlane.h"
#include "ProfileStore.h"
#include "StartupProfile.h"
#include "LiveEligibility.h"
#include "ConsoleShutdown.h"
#include "LifecycleExecutor.h"
#include "Engine/Egress/HardwareTest.h"
#include "DeviceService.h"
#include "Engine/AudioScheduler.h"
#include "IPC/CommandServer.h"
#include "IPC/TelemetryServer.h"
#include <iostream>
#include <filesystem>
#include <deque>
namespace entropy::control {
using J=ipc::Value;
namespace {ConsoleShutdown consoleShutdown;BOOL WINAPI consoleStop(DWORD event){return consoleShutdown.handle(event);}}
int runControlServer(int argc,char** argv) try {
 std::string endpoint="EntropyAudioCore.Control.v1",initialProfile,liveGatePath;std::wstring stage2Endpoint;bool signal=false,offlineSinks=false,stage2Enabled=false,authorizedLiveTest=false;unsigned seconds=0;
 for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--control-server")continue;else if(a=="--endpoint"&&i+1<argc)endpoint=argv[++i];else if(a=="--offline-test-signal")signal=true;else if(a=="--offline-test-sinks")offlineSinks=true;else if(a=="--stage2-input")stage2Enabled=true;else if(a=="--authorized-live-test")authorizedLiveTest=true;else if(a=="--stage2-endpoint"&&i+1<argc){std::string name=argv[++i];if(name.rfind("\\\\.\\pipe\\Entropy.StagePublish.V2.",0)!=0)throw std::invalid_argument("Input endpoint must be a local StagePublish V2 pipe");stage2Endpoint.assign(name.begin(),name.end());}else if(a=="--live-gate"&&i+1<argc)liveGatePath=argv[++i];else if(a=="--profile"&&i+1<argc)initialProfile=argv[++i];else if(a=="--run-seconds"&&i+1<argc){auto s=ipc::parse(argv[++i]);seconds=s.asUnsigned(86400);}else throw std::invalid_argument("Unknown control option: "+a);}
 if(authorizedLiveTest&&!stage2Enabled)throw std::invalid_argument("Authorized live test requires --stage2-input; offline fixtures cannot authorize physical output");
 if(stage2Enabled&&(signal||offlineSinks))throw std::invalid_argument("Real Stage2 and offline fixtures are mutually exclusive");
 auto barrier=[&](const ControlPlane& controls){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!controls.settled()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Profile barrier timed out; no state changed");std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 engine::AudioGraph graph;if(!graph.prepare(stage2Enabled?1024:256))throw std::runtime_error("Native graph prepare failed");graph.enableOriginalMusicAnalysis(true);ControlPlane controls(graph,GetTickCount64()+1);if(offlineSinks&&!signal)throw std::invalid_argument("Offline sinks require explicit offline signal");OfflineSinkWorkers sinkWorkers;auto liveOwner=std::make_unique<LiveSinkWorkers>();auto& liveWorkers=*liveOwner;auto stage2=std::make_unique<input::Stage2Ingress>();AudioScheduler scheduler(graph,controls,signal,offlineSinks?&sinkWorkers:nullptr,stage2Enabled?stage2.get():nullptr,&liveWorkers);DeviceService devices;ProfileStore profiles;ipc::CommandServer server;
 std::array<wchar_t,32768> currentExe{};GetModuleFileNameW(nullptr,currentExe.data(),unsigned(currentExe.size()));const bool liveGate=stage2Enabled&&!liveGatePath.empty()&&liveGateMatches(std::filesystem::u8path(liveGatePath),egress_host::fileSha256(currentExe.data()));
 struct ReplyEntry{ipc::Frame request,response;};std::deque<ReplyEntry> replies;uint64_t session=GetTickCount64()+1;
 auto configuredMask=[&]{return offlineSinks?0xffffu:LiveSinkWorkers::renderLaneMask(LiveSinkWorkers::decode(devices.snapshot().at("configuration")));};
 controls.setRenderLaneMask(configuredMask());controls.atBlockBoundary();controls.afterBlock();
 auto deviceCommand=[&](const J& request,bool binding){
  const auto previous=devices.snapshot();
  try{auto result=binding?devices.bindingCommand(request):devices.configure(request);controls.setRenderLaneMask(configuredMask());return result;}
  catch(...){devices.restore(previous);throw;}
 };
 bool profileRequired=stage2Enabled;std::string profileError;
 if(initialProfile.empty()&&stage2Enabled)initialProfile=profilePathUtf8(motherProfilePath());
 if(!initialProfile.empty())try{auto loaded=profiles.load(std::filesystem::u8path(initialProfile));if(loaded.document.contains("stage05_devices"))devices.restore(loaded.document.at("stage05_devices"));loaded.parameters.renderLaneMask=configuredMask();controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();profiles.adopt(std::move(loaded.document));profileRequired=false;}
 catch(const std::exception& e){profileError=e.what();if(!stage2Enabled)throw;}
 const bool startInputImmediately=stage2Enabled&&!profileRequired;
 // Live Core is an independent process, not a child console's audio session.
 // Detach before any source lease is acquired. Redirected log files remain
 // the launcher's log destination; the UI continues to use IPC only.
 if(stage2Enabled&&!FreeConsole())throw std::runtime_error("Cannot detach live Core from launcher console");
 consoleShutdown.reset();scheduler.start();
 if(startInputImmediately&&!stage2->start(stage2Endpoint,stage2Endpoint.empty()))throw std::runtime_error("Stage2 input worker start failed");
 struct Cached {J description,telemetry,devices;};std::atomic<std::shared_ptr<const Cached>> cache;
 std::atomic<LifecycleExecutor*> lifecycle{nullptr};
 auto publishCache=[&]{
  auto next=std::make_shared<Cached>();
  next->description=controls.metadata();auto& description=next->description;
  description["viewContractVersion"]=2;description["protocol"]=1;description["physicalArmAllowed"]=liveGate||authorizedLiveTest;
  description["offlineGatePassed"]=liveGate;description["authorizedLiveTest"]=authorizedLiveTest;description["owner"]="EntropyAudioCore";
  description["profileRequired"]=profileRequired;description["profilePath"]=initialProfile;description["profileError"]=profileError;
  DWORD consolePid=0;description["consoleAttached"]=GetConsoleProcessList(&consolePid,1)!=0;
  auto& data=next->telemetry;data=ipc::telemetrySnapshot(controls,scheduler,signal,stage2Enabled?stage2.get():nullptr);
  if(offlineSinks)data["domains"]=sinkWorkers.snapshot();else data["domains"]=liveWorkers.snapshot();
  data["armed"]=liveWorkers.armed();data["armPending"]=liveWorkers.engaged()&&!liveWorkers.armed()&&!liveWorkers.faulted();data["liveFault"]=liveWorkers.faulted();
  data["offlineGatePassed"]=liveGate;data["authorizedLiveTest"]=authorizedLiveTest;
  data["hardwareGate"]=authorizedLiveTest?"USER AUTHORIZED LIVE TEST; offline PASS not claimed; explicit ARM and profile/device validation required":liveGate?"OFFLINE GATES PASS; explicit ARM and profile/device validation required":"BLOCKED: Stage04 live offline gate missing/stale";
  data["profileRequired"]=profileRequired;data["profilePath"]=initialProfile;data["profileError"]=profileError;
  data["benchmarkCalls"]=J::u64(diagnosticBenchmarkCalls());data["preparationCache"]=controls.crossover().preparationCache();
  if(profileRequired)data["inputState"]="WAITING FOR PROFILE";
  next->devices=devices.snapshot();
  cache.store(std::shared_ptr<const Cached>(std::move(next)));
  if(auto* owner=lifecycle.load()){
   const std::string state=owner->stateText();
   if(state=="ARMING"||state=="RUNNING")owner->transition(state=="ARMING"?LifecycleExecutor::State::Arming:LifecycleExecutor::State::Running,liveWorkers.faulted()?LifecycleExecutor::State::Fault:liveWorkers.armed()?LifecycleExecutor::State::Running:LifecycleExecutor::State::Arming);
  }
 };
 publishCache();
 auto apply=[&](const J& request,uint64_t generation)->J{
  auto action=request.at("action").asString();if(action=="save-profile"&&profileRequired)action="new-profile";
  if(action=="offline-session-close"){sinkWorkers.stop();return J::Object{{"status","DISARMED"}};}
  J data;
   if(action=="devices")data=devices.refresh();
   else if(action=="approve-binding"||action=="clear-binding"){sinkWorkers.stop();scheduler.disarm();data=deviceCommand(request,true);}
   else if(action=="configure-devices"){sinkWorkers.stop();scheduler.disarm();data=deviceCommand(request,false);}
   else if(action=="offline-arm"){if(!offlineSinks)throw std::invalid_argument("Offline sink fixture not enabled");auto state=controls.appliedState();if(state.mode!=ENTROPY_HEADROOM_MODE_MANUAL_SAFE||state.manualGainDb>-6)throw std::invalid_argument("Offline fixture requires explicit safe Headroom policy");scheduler.stop();try{sinkWorkers.start();}catch(...){scheduler.start();throw;}scheduler.start();data=J::Object{{"offlineArmed",true},{"physicalArmed",false}};}
   else if(action=="arm") {
    if(!liveGate&&!authorizedLiveTest)throw std::invalid_argument("Physical ARM BLOCKED: Stage04 live offline gate missing/stale; no sink opened");
    if(profileRequired||stage2->telemetry().state!=input::State::Receiving)throw std::invalid_argument("Validated profile and active FP64 input transport required");
    if(liveWorkers.engaged())throw std::invalid_argument("Already armed/pending/faulted; DISARM before a new ARM");
    if(request.contains("domains"))deviceCommand(request,false);
    if(!lifecycle.load()->current(generation))throw std::runtime_error("ARM canceled by DISARM");
    lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Preparing);
    devices.revalidateSelected();
    if(!liveFirAllowed(controls.crossover().preparationParameters())||!controls.crossover().preparedFinite())throw std::invalid_argument("Unprepared/nonfinite or unauthorized FIR profile; no device opened");
    auto config=LiveSinkWorkers::decode(devices.snapshot().at("configuration"));
    barrier(controls);
    if(!liveHeadroomReady(graph))throw std::invalid_argument("Headroom reports an original module fault or unsettled transition; no device opened");
    if(!liveWorkers.prepare(config))throw std::runtime_error("Live egress prepare failed");
    try{if(!lifecycle.load()->activate(generation,[&]{return liveWorkers.start();}))throw std::runtime_error("ARM canceled or egress start failed");}
    catch(...){liveWorkers.stop();throw;}
    data=J::Object{{"status","ARMING"},{"physicalArmed",false},{"prepared",controls.crossover().preparationCache()}};
   }
   else if(action=="disarm"){sinkWorkers.stop();scheduler.disarm();data=J::Object{{"armed",false}};}
   else if(action=="save-profile"){
    if(profileRequired)throw std::invalid_argument("Create or load a profile first");
    if(stage2Enabled&&liveWorkers.armed()){barrier(controls);profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot());}
    else if(stage2Enabled){scheduler.stop();scheduler.disarm();try{controls.drainAtQuiescentBoundary();profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot());}catch(...){scheduler.start();throw;}scheduler.start();}
    else {barrier(controls);profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot());}
    data=J::Object{{"status","SAVED"},{"audioApplicationPending",!controls.crossover().settled()}};
   }
   else if(action=="new-profile"){
    if(!profileRequired)throw std::invalid_argument("A profile is already active; use Save Project");auto path=std::filesystem::u8path(request.at("path").asString());scheduler.stop();
    try{controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});controls.atBlockBoundary();controls.afterBlock();profiles.save(path,controls,devices.snapshot());initialProfile=profilePathUtf8(path);profileRequired=false;profileError.clear();if(stage2Enabled&&!stage2->start(stage2Endpoint,stage2Endpoint.empty()))throw std::runtime_error("Stage2 input worker start failed");scheduler.start();}
    catch(...){scheduler.start();throw;}data=J::Object{{"status","CREATED"},{"devices",devices.snapshot()}};
   }
   else if(action=="load-profile"){
    auto loaded=profiles.load(std::filesystem::u8path(request.at("path").asString()));if(!profileRequired&&!stage2Enabled)barrier(controls);scheduler.stop();scheduler.disarm();if(stage2Enabled)controls.drainAtQuiescentBoundary();sinkWorkers.stop();auto oldDevices=devices.snapshot();auto old=controls.crossover().exportBytes();auto state=controls.appliedState();
    try{if(loaded.document.contains("stage05_devices"))devices.restore(loaded.document.at("stage05_devices"));loaded.parameters.renderLaneMask=configuredMask();controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();profiles.adopt(std::move(loaded.document));if(profileRequired&&stage2Enabled&&!stage2->start(stage2Endpoint,stage2Endpoint.empty()))throw std::runtime_error("Stage2 input worker start failed");profileRequired=false;profileError.clear();initialProfile=request.at("path").asString();scheduler.start();}
    catch(...){devices.restore(oldDevices);controls.crossover().importBytes(old);controls.replace(state);controls.atBlockBoundary();controls.afterBlock();scheduler.start();throw;}
    data=J::Object{{"status","LOADED"},{"devices",devices.snapshot()}};
   }else {
    if(profileRequired)throw std::invalid_argument("Create or load a validated profile first");
    if(liveWorkers.engaged()){
     if(liveSceneChangeRequested(request)){
      lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Preparing);
      if(!liveFirAllowed(controls.crossover().preparationParameters(request)))throw std::invalid_argument("FIR8193 x16 is not authorized for live");
     }else if(!liveCrossoverChangeAllowed(controls.crossover().preparationParameters(),request))throw std::invalid_argument("FIR8193 x16 is not authorized for live; DISARM before changing this configuration");
    }
    auto sequence=controls.execute(request);
    if(!controls.crossover().preparedFinite())throw std::invalid_argument("Crossover coefficient preparation failed");
    data=J::Object{{"accepted",J::u64(sequence)},{"status","ACCEPTED_AT_CONTROL"},{"audioApplied",false}};
   }
  if(lifecycle.load()->current(generation)){
   if(action=="arm")lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Arming);
   else if(action=="disarm"||action=="load-profile"||action=="configure-devices"||action=="approve-binding"||action=="clear-binding")lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Disarmed);
   else lifecycle.load()->stateIfCurrent(generation,liveWorkers.armed()?LifecycleExecutor::State::Running:liveWorkers.engaged()?LifecycleExecutor::State::Arming:LifecycleExecutor::State::Disarmed);
  }
  return data;
 };
 LifecycleExecutor executor(apply,[&]{liveWorkers.closePublicationGate();},publishCache,[&]{devices.closeNotifications();});lifecycle.store(&executor);
 std::string error;
 if(!server.start(std::wstring(endpoint.begin(),endpoint.end()),[&](const ipc::Frame& frame){
  for(const auto& entry:replies)if(entry.request.requestId==frame.requestId){if(entry.request.command==frame.command&&entry.request.payload==frame.payload)return entry.response;return ipc::Frame{frame.command,frame.requestId,ipc::dump(J::Object{{"ok",false},{"session",J::u64(session)},{"error","Request ID reused with different payload"}})};}
  J reply;try{
   if(frame.command<1||frame.command>9)throw std::invalid_argument("Unknown opcode");
   auto request=ipc::parse(frame.payload);auto action=request.at("action").asString();
   if(request.contains("session")&&request.at("session").asU64()!=session)throw std::invalid_argument("Stale control session");
   J data;auto latest=cache.load();
   if(action=="describe")data=latest->description;
   else if(action=="telemetry"){
    data=latest->telemetry;
    if(!request.contains("viewContractVersion")||request.at("viewContractVersion").asUnsigned()!=2)data["view"]=J::Object{{"version",2},{"state","INCOMPATIBLE CLIENT"},{"spectrumAvailable",false},{"responseAvailable",false}};
    else{auto& view=data["view"];LARGE_INTEGER now{};QueryPerformanceCounter(&now);const auto captured=view.at("captureQpc").asU64();if(!captured||double(uint64_t(now.QuadPart)-captured)>view.at("qpcFrequency").asNumber()*.5){view["spectrumAvailable"]=false;view["state"]="STALE";}}
    data["lifecycleStatus"]=executor.stateText();
    if(std::string(executor.stateText())=="STOPPING"){data["armed"]=false;data["armPending"]=false;data["publicationGateClosed"]=true;}
   }else if(action=="command-results")data=executor.results(request.at("ids"));
   else if(action=="preflight-live")throw std::invalid_argument("Qualification is explicit offline only: EntropyLiveQualification.exe PROFILE.entropy [MUSIC|CINEMA]");
   else if(action=="shutdown"){
    executor.admit(J::Object{{"action","disarm"}},true);server.requestStopAfterReply();data=J::Object{{"status","STOPPING"},{"armed",false}};
   }else{
    if(action=="arm"&&!liveGate&&!authorizedLiveTest)throw std::invalid_argument("Physical ARM BLOCKED: Stage04 live offline gate missing/stale; no sink opened");
    const bool cancel=action=="disarm"||action=="load-profile"||action=="configure-devices"||action=="approve-binding"||action=="clear-binding";
    data=executor.admit(request,cancel);
   }
   reply=J::Object{{"ok",true},{"data",std::move(data)}};
  }catch(const std::exception& e){reply=J::Object{{"ok",false},{"error",e.what()}};}
  reply["session"]=J::u64(session);ipc::Frame response{frame.command,frame.requestId,ipc::dump(reply)};
  if(replies.size()==128)replies.pop_front();replies.push_back({frame,response});return response;
 },[&]{if(offlineSinks)executor.admit(J::Object{{"action","offline-session-close"}});replies.clear();++session;},error))throw std::runtime_error(error);
 SetConsoleCtrlHandler(consoleStop,TRUE);std::cout<<"Stage05 native IPC READY: "<<endpoint<<"; DISARMED; liveGate="<<(liveGate?"PASS":"BLOCKED")<<"; authorizedLiveTest="<<(authorizedLiveTest?"YES":"NO")<<"\n"<<std::flush;
 auto start=std::chrono::steady_clock::now();while(!consoleShutdown.requested()&&!server.shutdownReplyComplete()&&(!seconds||std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))std::this_thread::sleep_for(std::chrono::milliseconds(20));
 server.stop();executor.stop();scheduler.stop();liveWorkers.stop();stage2->stop();sinkWorkers.stop();std::cout<<"AudioCore stopped safely; all native DAC owners closed\n"<<std::flush;consoleShutdown.complete();SetConsoleCtrlHandler(consoleStop,FALSE);return 0;
}catch(const std::exception& e){std::cerr<<"Stage05 FAIL: "<<e.what()<<'\n';consoleShutdown.complete();SetConsoleCtrlHandler(consoleStop,FALSE);return 1;}
}
