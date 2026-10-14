#include "CoreRuntime.h"
#include "ControlPlane.h"
#include "ProfileStore.h"
#include "StartupProfile.h"
#include "LiveEligibility.h"
#include "ConsoleShutdown.h"
#include "LifecycleExecutor.h"
#include "SourceWaitPolicy.h"
#include "Engine/Egress/HardwareTest.h"
#include "DeviceService.h"
#include "Engine/AudioScheduler.h"
#include "Engine/Diagnostics/ChainAudit.h"
#include "Engine/Input/InputConfiguration.h"
#include "IPC/CommandServer.h"
#include "IPC/TelemetryServer.h"
#include <iostream>
#include <filesystem>
#include <deque>
namespace entropy::control {
using J=ipc::Value;
namespace {ConsoleShutdown consoleShutdown;BOOL WINAPI consoleStop(DWORD event){return consoleShutdown.handle(event);}}
int runControlServer(int argc,char** argv) try {
 struct AuditLifetime {AuditLifetime(){chain_audit::prepare();}~AuditLifetime(){chain_audit::finish();}} auditLifetime;
 std::string endpoint="EntropyAudioCore.Control.v1",initialProfile,liveGatePath;input::Configuration inputs;bool signal=false,offlineSinks=false,stage2Enabled=false,authorizedLiveTest=false,inputOnly=false,musicOverride=false,cinemaOverride=false;unsigned seconds=0;
 for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--control-server")continue;else if(a=="--endpoint"&&i+1<argc)endpoint=argv[++i];else if(a=="--offline-test-signal")signal=true;else if(a=="--offline-test-sinks")offlineSinks=true;else if(a=="--stage2-input")stage2Enabled=true;else if(a=="--input-only"){inputOnly=true;stage2Enabled=true;}else if(a=="--authorized-live-test")authorizedLiveTest=true;else if((a=="--stage2-endpoint"||a=="--music-endpoint"||a=="--cinema-endpoint")&&i+1<argc){std::string name=argv[++i];std::wstring wide(name.begin(),name.end());if(!input::localEndpoint(wide))throw std::invalid_argument("Input endpoint must be a local named pipe");if(a=="--cinema-endpoint"){inputs.cinema=wide;cinemaOverride=true;}else{inputs.music=wide;musicOverride=true;}}else if(a=="--live-gate"&&i+1<argc)liveGatePath=argv[++i];else if(a=="--profile"&&i+1<argc)initialProfile=argv[++i];else if(a=="--run-seconds"&&i+1<argc){auto s=ipc::parse(argv[++i]);seconds=s.asUnsigned(86400);}else throw std::invalid_argument("Unknown control option: "+a);}
 if(inputOnly&&(authorizedLiveTest||!liveGatePath.empty()))throw std::invalid_argument("Input-only cannot authorize physical output or use a live gate");
 if(authorizedLiveTest&&!stage2Enabled)throw std::invalid_argument("Authorized live test requires --stage2-input; offline fixtures cannot authorize physical output");
 if(stage2Enabled&&(signal||offlineSinks))throw std::invalid_argument("Real Stage2 and offline fixtures are mutually exclusive");
 auto barrier=[&](const ControlPlane& controls){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!controls.settled()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Profile barrier timed out; no state changed");std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 engine::AudioGraph graph;if(!graph.prepare(stage2Enabled?1024:256))throw std::runtime_error("Native graph prepare failed");graph.enableOriginalMusicAnalysis(true);ControlPlane controls(graph,GetTickCount64()+1);if(offlineSinks&&!signal)throw std::invalid_argument("Offline sinks require explicit offline signal");OfflineSinkWorkers sinkWorkers;auto liveOwner=std::make_unique<LiveSinkWorkers>();auto& liveWorkers=*liveOwner;auto stage2=std::make_unique<input::ProgramIngress>();input::ProgramSelection selection;auto schedulerOwner=stage2Enabled?std::make_unique<AudioScheduler>(graph,controls,*stage2,selection,&liveWorkers):std::make_unique<AudioScheduler>(graph,controls,signal,offlineSinks?&sinkWorkers:nullptr,nullptr,&liveWorkers);auto& scheduler=*schedulerOwner;DeviceService devices;ProfileStore profiles;ipc::CommandServer server;
 std::array<wchar_t,32768> currentExe{};GetModuleFileNameW(nullptr,currentExe.data(),unsigned(currentExe.size()));const bool liveGate=stage2Enabled&&!liveGatePath.empty()&&liveGateMatches(std::filesystem::u8path(liveGatePath),egress_host::fileSha256(currentExe.data()));
 struct ReplyEntry{ipc::Frame request,response;};std::deque<ReplyEntry> replies;uint64_t session=GetTickCount64()+1;
 auto configuredMask=[&]{return offlineSinks?0xffffu:LiveSinkWorkers::renderLaneMask(LiveSinkWorkers::decode(devices.snapshot().at("configuration")));};
 controls.setRenderLaneMask(configuredMask());controls.atBlockBoundary();controls.afterBlock();
 auto deviceCommand=[&](const J& request,bool binding){
  const auto previous=devices.snapshot();
  try{auto result=binding?devices.bindingCommand(request):devices.configure(request);controls.setRenderLaneMask(configuredMask());return result;}
  catch(...){devices.restore(previous);throw;}
 };
 bool profileRequired=stage2Enabled&&(!inputOnly||!initialProfile.empty());std::string profileError;
 if(initialProfile.empty()&&stage2Enabled&&!inputOnly)initialProfile=profilePathUtf8(motherProfilePath());
 if(!initialProfile.empty())try{auto loaded=profiles.load(std::filesystem::u8path(initialProfile));auto configured=input::Configuration::decode(loaded.document);if(!musicOverride)inputs.music=configured.music;if(!cinemaOverride)inputs.cinema=configured.cinema;if(loaded.document.contains("stage05_devices"))devices.restore(loaded.document.at("stage05_devices"));loaded.parameters.renderLaneMask=configuredMask();controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();profiles.adopt(std::move(loaded.document));profileRequired=false;}
 catch(const std::exception& e){profileError=e.what();if(!stage2Enabled)throw;}
 const bool startInputImmediately=stage2Enabled&&!profileRequired;
 // Existing independent Core lifecycle, separate IPC-only UI.
 if(stage2Enabled&&!FreeConsole())throw std::runtime_error("Cannot detach live Core from launcher console");
 consoleShutdown.reset();
 selection.quiescent(controls.crossover().snapshot().at("program").asUnsigned(1));stage2->require(selection.requiredDomains());scheduler.start();
 if(startInputImmediately&&!stage2->start(inputs.music,inputs.cinema,!inputOnly))throw std::runtime_error("Input start refused: render owner already exists or invalid local endpoint");
 struct Cached {J description,telemetry,devices;};std::atomic<std::shared_ptr<const Cached>> cache;
 std::atomic<LifecycleExecutor*> lifecycle{nullptr};
 SourceWaitPolicy sourceWait(liveWorkers);std::string inputResumeError;
 auto publishCache=[&]{
  if(auto* owner=lifecycle.load();owner&&sourceWait.requested()) {
   const auto generation=owner->generation();
   const auto source=stage2->telemetry(selection.program()?input::Domain::Cinema:input::Domain::Music);
   try {
    const bool eligible=!profileRequired&&controls.settled()&&liveFirAllowed(controls.crossover().preparationParameters())&&controls.crossover().preparedFinite()&&liveHeadroomReady(graph);
    const auto result=sourceWait.service(source.state==input::State::Receiving,source.lastBlockQpc,eligible,[&]{
     owner->stateIfCurrent(generation,LifecycleExecutor::State::Preparing);
     const auto playback=devices.independentPlaybackConfiguration();
     if(!sourceWait.requested()||!owner->current(generation))return false;
     if(!liveWorkers.prepare(LiveSinkWorkers::decode(playback)))return false;
     return owner->activate(generation,[&]{return sourceWait.requested()&&liveWorkers.start();});
    });
    if(result==SourceWaitPolicy::Result::Resumed){inputResumeError.clear();owner->stateIfCurrent(generation,LifecycleExecutor::State::Arming);}
    if(result==SourceWaitPolicy::Result::Faulted){inputResumeError.clear();owner->disarmAfterFault();}
    if(result==SourceWaitPolicy::Result::Failed){inputResumeError="Source resume failed; playback remains stopped";owner->stateIfCurrent(generation,LifecycleExecutor::State::Fault);}
   }catch(const std::exception& e){sourceWait.cancel();liveWorkers.stop();inputResumeError=e.what();owner->stateIfCurrent(generation,LifecycleExecutor::State::Fault);}
   if(sourceWait.muted())owner->stateIfCurrent(generation,LifecycleExecutor::State::WaitingInput);
  }
  auto next=std::make_shared<Cached>();
  next->description=controls.metadata();auto& description=next->description;
  description["viewContractVersion"]=2;description["protocol"]=1;description["physicalArmAllowed"]=!inputOnly&&(liveGate||authorizedLiveTest);description["version"]="0.2.0-live-v1";description["inputOnly"]=inputOnly;description["inputContractVersion"]=1;
  description["offlineGatePassed"]=liveGate;description["authorizedLiveTest"]=authorizedLiveTest;description["owner"]="EntropyAudioCore";
  description["profileRequired"]=profileRequired;description["profilePath"]=initialProfile;description["profileError"]=profileError;
  DWORD consolePid=0;description["consoleAttached"]=GetConsoleProcessList(&consolePid,1)!=0;
  auto& data=next->telemetry;data=stage2Enabled?ipc::programTelemetrySnapshot(controls,scheduler,*stage2,selection,inputs.music,inputs.cinema):ipc::telemetrySnapshot(controls,scheduler,signal,nullptr);data["version"]="0.2.0-live-v1";data["inputOnly"]=inputOnly;
  if(offlineSinks)data["domains"]=sinkWorkers.snapshot();else data["domains"]=liveWorkers.snapshot();
  data["physicalArmed"]=liveWorkers.armed();data["armed"]=sourceWait.requested();data["inputMuted"]=sourceWait.muted();
  data["sourceWaitCount"]=J::u64(sourceWait.waits());data["sourceResumeCount"]=J::u64(sourceWait.resumes());data["lastSourceWaitCause"]=sourceWait.lastSourceCause();data["inputResumeError"]=inputResumeError;
  data["armPending"]=liveWorkers.engaged()&&!liveWorkers.armed()&&!liveWorkers.faulted();data["liveFault"]=liveWorkers.faulted()&&!liveWorkers.sourceWaitFault();
  if(sourceWait.muted())data["publicationGateClosed"]=true;
  if(liveWorkers.sourceWaitFault())for(auto& domain:data["domains"].asArray()){
   domain["sourceWaitRetainedFaults"]=domain.at("faults");domain["faults"]=0;
   domain["state"]=sourceWait.muted()?"MUTE / WAITING INPUT":"DISARMED (source wait history retained)";
  }
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
    if(inputOnly)throw std::invalid_argument("Physical ARM BLOCKED: input-only validation; no sink opened");
    if(!liveGate&&!authorizedLiveTest)throw std::invalid_argument("Physical ARM BLOCKED: Stage04 live offline gate missing/stale; no sink opened");
    if(profileRequired)throw std::invalid_argument("Validated profile required");
    if(liveWorkers.engaged()||sourceWait.requested())throw std::invalid_argument("Already armed/pending/waiting; DISARM before a new ARM");
    if(request.contains("domains"))deviceCommand(request,false);
    if(!lifecycle.load()->current(generation))throw std::runtime_error("ARM canceled by DISARM");
    lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Preparing);
    const auto playback=devices.independentPlaybackConfiguration();
    if(!liveFirAllowed(controls.crossover().preparationParameters())||!controls.crossover().preparedFinite())throw std::invalid_argument("Unprepared/nonfinite or unauthorized FIR profile; no device opened");
    auto config=LiveSinkWorkers::decode(playback);
    if(stage2->telemetry(selection.program()?input::Domain::Cinema:input::Domain::Music).state!=input::State::Receiving){
     if(!liveWorkers.prepare(config))throw std::runtime_error("Live egress prepare failed");
     if(!lifecycle.load()->activate(generation,[&]{sourceWait.requestArm(true);return true;}))throw std::runtime_error("ARM canceled by DISARM");
     inputResumeError.clear();data=J::Object{{"status","MUTE_WAITING_INPUT"},{"physicalArmed",false}};
     lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::WaitingInput);return data;
    }
    barrier(controls);
    if(!liveHeadroomReady(graph))throw std::invalid_argument("Headroom reports an original module fault or unsettled transition; no device opened");
    if(!liveWorkers.prepare(config))throw std::runtime_error("Live egress prepare failed");
    try{if(!lifecycle.load()->activate(generation,[&]{const bool started=liveWorkers.start();if(started)sourceWait.requestArm();return started;}))throw std::runtime_error("ARM canceled or egress start failed");}
    catch(...){liveWorkers.stop();throw;}
    data=J::Object{{"status","ARMING"},{"physicalArmed",false},{"prepared",controls.crossover().preparationCache()}};
   }
   else if(action=="disarm"){sourceWait.cancel();sinkWorkers.stop();scheduler.disarm();data=J::Object{{"armed",false}};}
   else if(action=="save-profile"){
    if(profileRequired)throw std::invalid_argument("Create or load a profile first");
    if(stage2Enabled&&liveWorkers.armed()){barrier(controls);profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot(),stage2Enabled?inputs.encode():J{});}
    else if(stage2Enabled){scheduler.stop();scheduler.disarm();try{controls.drainAtQuiescentBoundary();profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot(),stage2Enabled?inputs.encode():J{});}catch(...){scheduler.start();throw;}scheduler.start();}
    else {barrier(controls);profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot(),stage2Enabled?inputs.encode():J{});}
    data=J::Object{{"status","SAVED"},{"audioApplicationPending",!controls.crossover().settled()}};
   }
   else if(action=="new-profile"){
    if(!profileRequired)throw std::invalid_argument("A profile is already active; use Save Project");auto path=std::filesystem::u8path(request.at("path").asString());scheduler.stop();
    try{controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});controls.atBlockBoundary();controls.afterBlock();profiles.save(path,controls,devices.snapshot(),stage2Enabled?inputs.encode():J{});initialProfile=profilePathUtf8(path);profileRequired=false;profileError.clear();if(stage2Enabled&&!stage2->start(inputs.music,inputs.cinema,!inputOnly))throw std::runtime_error("Stage2 input worker start failed");scheduler.start();}
    catch(...){scheduler.start();throw;}data=J::Object{{"status","CREATED"},{"devices",devices.snapshot()}};
   }
   else if(action=="load-profile"){
    auto loaded=profiles.load(std::filesystem::u8path(request.at("path").asString()));auto loadedInputs=input::Configuration::decode(loaded.document);if(musicOverride)loadedInputs.music=inputs.music;if(cinemaOverride)loadedInputs.cinema=inputs.cinema;if(!profileRequired&&!stage2Enabled)barrier(controls);scheduler.stop();scheduler.disarm();if(stage2Enabled)controls.drainAtQuiescentBoundary();sinkWorkers.stop();auto oldDevices=devices.snapshot();auto old=controls.crossover().exportBytes();auto state=controls.appliedState();auto oldInputs=inputs;auto oldProgram=selection.program();
    try{if(loaded.document.contains("stage05_devices"))devices.restore(loaded.document.at("stage05_devices"));loaded.parameters.renderLaneMask=configuredMask();controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();if(stage2Enabled){inputs=loadedInputs;selection.quiescent(controls.crossover().snapshot().at("program").asUnsigned(1));stage2->require(selection.requiredDomains());if(!profileRequired&&!stage2->configure(inputs.music,inputs.cinema))throw std::runtime_error("Input configuration failed");}profiles.adopt(std::move(loaded.document));if(profileRequired&&stage2Enabled&&!stage2->start(inputs.music,inputs.cinema,!inputOnly))throw std::runtime_error("Stage2 input worker start failed");profileRequired=false;profileError.clear();initialProfile=request.at("path").asString();scheduler.start();}
    catch(...){inputs=oldInputs;selection.quiescent(oldProgram);if(stage2Enabled&&!profileRequired)stage2->configure(inputs.music,inputs.cinema);devices.restore(oldDevices);controls.crossover().importBytes(old);controls.replace(state);controls.atBlockBoundary();controls.afterBlock();scheduler.start();throw;}
    data=J::Object{{"status","LOADED"},{"devices",devices.snapshot()}};
   }else {
    if(profileRequired)throw std::invalid_argument("Create or load a validated profile first");
    if(liveWorkers.engaged()){
     if(liveSceneChangeRequested(request)){
      lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Preparing);
      if(!liveFirAllowed(controls.crossover().preparationParameters(request)))throw std::invalid_argument("FIR8193 x16 is not authorized for live");
     }else if(!liveCrossoverChangeAllowed(controls.crossover().preparationParameters(),request))throw std::invalid_argument("FIR8193 x16 is not authorized for live; DISARM before changing this configuration");
    }
    uint32_t target=selection.program();auto proposed=controls.crossover().preparationParameters((action=="program"||action=="parameter"||action=="parameters")?request:J{});for(const auto& p:proposed.at("parameters").asArray())if(p.at("id").asString()=="cinemaSource")target=p.at("value").asUnsigned(1);
    auto previousSelection=selection.snapshot();
    if(stage2Enabled){std::string reason;if(!selection.request(target,*stage2,reason))throw std::invalid_argument(reason);if(target!=selection.program()){if(stage2->telemetry(selection.program()?input::Domain::Cinema:input::Domain::Music).state!=input::State::Receiving)selection.quiescent(target);else selection.accepted(target);stage2->require(selection.requiredDomains());}}
    uint64_t sequence=0;try{sequence=controls.execute(request);}catch(...){selection.restore(previousSelection);stage2->require(selection.requiredDomains());throw;}
    if(!controls.crossover().preparedFinite())throw std::invalid_argument("Crossover coefficient preparation failed");
    data=J::Object{{"accepted",J::u64(sequence)},{"status","ACCEPTED_AT_CONTROL"},{"audioApplied",false}};
   }
  if(lifecycle.load()->current(generation)){
   if(action=="arm")lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Arming);
   else if(action=="disarm"||action=="load-profile"||action=="configure-devices"||action=="approve-binding"||action=="clear-binding")lifecycle.load()->stateIfCurrent(generation,LifecycleExecutor::State::Disarmed);
   else lifecycle.load()->stateIfCurrent(generation,sourceWait.muted()?LifecycleExecutor::State::WaitingInput:liveWorkers.armed()?LifecycleExecutor::State::Running:liveWorkers.engaged()?LifecycleExecutor::State::Arming:LifecycleExecutor::State::Disarmed);
  }
  return data;
 };
 LifecycleExecutor executor(apply,[&]{sourceWait.cancel();liveWorkers.closePublicationGate();},publishCache,[&]{devices.closeNotifications();},[&]{return liveWorkers.faulted()&&liveWorkers.engaged()&&!liveWorkers.sourceWaitFault();});lifecycle.store(&executor);
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
    if(action=="arm"&&(inputOnly||(!liveGate&&!authorizedLiveTest)))throw std::invalid_argument(inputOnly?"Physical ARM BLOCKED: input-only validation; no sink opened":"Physical ARM BLOCKED: Stage04 live offline gate missing/stale; no sink opened");
    const bool cancel=action=="disarm"||action=="load-profile"||action=="configure-devices"||action=="approve-binding"||action=="clear-binding";
    data=executor.admit(request,cancel);
   }
   reply=J::Object{{"ok",true},{"data",std::move(data)}};
  }catch(const std::exception& e){reply=J::Object{{"ok",false},{"error",e.what()}};}
  reply["session"]=J::u64(session);ipc::Frame response{frame.command,frame.requestId,ipc::dump(reply)};
  if(replies.size()==128)replies.pop_front();replies.push_back({frame,response});return response;
 },[&]{if(offlineSinks)executor.admit(J::Object{{"action","offline-session-close"}});replies.clear();++session;},error))throw std::runtime_error(error);
 SetConsoleCtrlHandler(consoleStop,TRUE);std::cout<<"Stage05 native IPC READY: "<<endpoint<<"; DISARMED; renderer=LiveV1; liveGate="<<(liveGate?"PASS":"BLOCKED")<<"; authorizedLiveTest="<<(authorizedLiveTest?"YES":"NO")<<"\n"<<std::flush;
 auto start=std::chrono::steady_clock::now();while(!consoleShutdown.requested()&&!server.shutdownReplyComplete()&&(!seconds||std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))std::this_thread::sleep_for(std::chrono::milliseconds(20));
 server.stop();executor.stop();scheduler.stop();liveWorkers.stop();stage2->stop();sinkWorkers.stop();std::cout<<"AudioCore stopped safely; all native DAC owners closed\n"<<std::flush;consoleShutdown.complete();SetConsoleCtrlHandler(consoleStop,FALSE);return 0;
}catch(const std::exception& e){std::cerr<<"Stage05 FAIL: "<<e.what()<<'\n';consoleShutdown.complete();SetConsoleCtrlHandler(consoleStop,FALSE);return 1;}
}
