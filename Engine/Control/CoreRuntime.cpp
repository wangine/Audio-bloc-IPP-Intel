#include "CoreRuntime.h"
#include "ControlPlane.h"
#include "ProfileStore.h"
#include "DeviceService.h"
#include "Engine/AudioScheduler.h"
#include "IPC/CommandServer.h"
#include "IPC/TelemetryServer.h"
#include <iostream>
#include <filesystem>
#include <deque>
namespace entropy::control {
using J=ipc::Value;
namespace {std::atomic<bool> exitRequested{false};BOOL WINAPI consoleStop(DWORD event){if(event==CTRL_C_EVENT||event==CTRL_BREAK_EVENT||event==CTRL_CLOSE_EVENT){exitRequested=true;return TRUE;}return FALSE;}}
int runControlServer(int argc,char** argv) try {
 std::string endpoint="EntropyAudioCore.Control.v1";bool signal=false,offlineSinks=false;unsigned seconds=0;
 for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--control-server")continue;else if(a=="--endpoint"&&i+1<argc)endpoint=argv[++i];else if(a=="--offline-test-signal")signal=true;else if(a=="--offline-test-sinks")offlineSinks=true;else if(a=="--run-seconds"&&i+1<argc){auto s=ipc::parse(argv[++i]);seconds=s.asUnsigned(86400);}else throw std::invalid_argument("Unknown control option: "+a);}
 auto barrier=[&](const ControlPlane& controls){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!controls.settled()){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Profile barrier timed out; no state changed");std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 engine::AudioGraph graph;if(!graph.prepare(256))throw std::runtime_error("Native graph prepare failed");graph.enableOriginalMusicAnalysis(true);ControlPlane controls(graph,GetTickCount64()+1);if(offlineSinks&&!signal)throw std::invalid_argument("Offline sinks require explicit offline signal");OfflineSinkWorkers sinkWorkers;AudioScheduler scheduler(graph,controls,signal,offlineSinks?&sinkWorkers:nullptr);DeviceService devices;ProfileStore profiles;ipc::CommandServer server;
 struct ReplyEntry{ipc::Frame request,response;};std::deque<ReplyEntry> replies;uint64_t session=GetTickCount64()+1;
 std::string error;scheduler.start();if(!server.start(std::wstring(endpoint.begin(),endpoint.end()),[&](const ipc::Frame& frame){
  for(const auto& entry:replies)if(entry.request.requestId==frame.requestId){if(entry.request.command==frame.command&&entry.request.payload==frame.payload)return entry.response;return ipc::Frame{frame.command,frame.requestId,ipc::dump(J::Object{{"ok",false},{"session",J::u64(session)},{"error","Request ID reused with different payload"}})};}
  J reply;try{if(frame.command<1||frame.command>9)throw std::invalid_argument("Unknown opcode");auto request=ipc::parse(frame.payload);auto action=request.at("action").asString();if(request.contains("session")&&request.at("session").asU64()!=session)throw std::invalid_argument("Stale control session");J data;
   if(action=="describe"){data=controls.metadata();data["protocol"]=1;data["physicalArmAllowed"]=false;data["owner"]="EntropyAudioCore";}
   else if(action=="telemetry"){data=ipc::telemetrySnapshot(controls,scheduler,signal);if(offlineSinks)data["domains"]=sinkWorkers.snapshot();}
   else if(action=="devices")data=devices.refresh();
   else if(action=="approve-binding"||action=="clear-binding"){sinkWorkers.stop();scheduler.disarm();data=devices.bindingCommand(request);}
   else if(action=="configure-devices"){sinkWorkers.stop();scheduler.disarm();data=devices.configure(request);}
   else if(action=="offline-arm"){if(!offlineSinks)throw std::invalid_argument("Offline sink fixture not enabled");auto state=controls.appliedState();if(state.mode!=ENTROPY_HEADROOM_MODE_MANUAL_SAFE||state.manualGainDb>-6)throw std::invalid_argument("Offline fixture requires explicit safe Headroom policy");scheduler.stop();try{sinkWorkers.start();}catch(...){scheduler.start();throw;}scheduler.start();data=J::Object{{"offlineArmed",true},{"physicalArmed",false}};}
   else if(action=="arm")throw std::invalid_argument("Physical ARM BLOCKED: Stage04 FAIL REALTIME; no sink opened");
   else if(action=="disarm"){sinkWorkers.stop();scheduler.disarm();data=J::Object{{"armed",false}};}
   else if(action=="save-profile"){barrier(controls);profiles.save(std::filesystem::u8path(request.at("path").asString()),controls,devices.snapshot());data=J::Object{{"status","SAVED"}};}
   else if(action=="load-profile"){
    auto loaded=profiles.load(std::filesystem::u8path(request.at("path").asString()));barrier(controls);scheduler.stop();sinkWorkers.stop();auto oldDevices=devices.snapshot();auto old=controls.crossover().exportBytes();auto state=controls.appliedState();
    try{if(loaded.document.contains("stage05_devices"))devices.restore(loaded.document.at("stage05_devices"));controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();profiles.adopt(std::move(loaded.document));scheduler.start();}
    catch(...){devices.restore(oldDevices);controls.crossover().importBytes(old);controls.replace(state);controls.atBlockBoundary();controls.afterBlock();scheduler.start();throw;}
    data=J::Object{{"status","LOADED"},{"devices",devices.snapshot()}};
   }else {auto sequence=controls.execute(request);data=J::Object{{"accepted",J::u64(sequence)}};}
   reply=J::Object{{"ok",true},{"data",std::move(data)}};
  }catch(const std::exception& e){reply=J::Object{{"ok",false},{"error",e.what()}};}
  reply["session"]=J::u64(session);ipc::Frame response{frame.command,frame.requestId,ipc::dump(reply)};if(replies.size()==128)replies.pop_front();replies.push_back({frame,response});return response;
 },[&]{sinkWorkers.stop();scheduler.disarm();replies.clear();++session;},error))throw std::runtime_error(error);
 exitRequested=false;SetConsoleCtrlHandler(consoleStop,TRUE);std::cout<<"Stage05 native IPC READY: "<<endpoint<<"; DISARMED; Stage04 FAIL REALTIME / hardware BLOCKED\n"<<std::flush;
 auto start=std::chrono::steady_clock::now();while(!exitRequested.load()&&(!seconds||std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))std::this_thread::sleep_for(std::chrono::milliseconds(20));
 server.stop();scheduler.stop();sinkWorkers.stop();SetConsoleCtrlHandler(consoleStop,FALSE);std::cout<<"Stage05 stopped safely; no physical DAC opened\n";return 0;
}catch(const std::exception& e){std::cerr<<"Stage05 FAIL: "<<e.what()<<'\n';return 1;}
}
