#include "TelemetryServer.h"
#include <windows.h>
namespace entropy::ipc {
Value programTelemetrySnapshot(const control::ControlPlane& controls,const control::AudioScheduler& scheduler,const input::ProgramIngress& in,const input::ProgramSelection& selection,const std::wstring& music,const std::wstring& cinema){
 auto v=telemetrySnapshot(controls,scheduler,false,nullptr);v["inputContractVersion"]=1;v["selectedProgram"]=selection.program();v["blockSize"]=1024;
 auto encode=[](const input::DomainTelemetry& s,const std::wstring& endpoint){std::string uuid;constexpr char hex[]="0123456789abcdef";for(auto b:s.sourceSession){uuid+=hex[b>>4];uuid+=hex[b&15];}Value::Array maximumPeaks;for(unsigned c=0;c<s.session.channels;++c)maximumPeaks.emplace_back(s.maximumPeaks[c]);Value::Array peaks,generations,rates,states;for(unsigned c=0;c<s.session.channels;++c)peaks.emplace_back(s.state==input::State::Receiving?s.peaks[c]:0.0);for(unsigned i=0;i<3;++i){generations.emplace_back(Value::u64(s.block.source_generation[i]));rates.emplace_back(s.block.source_rate[i]);states.emplace_back(s.block.source_state[i]);}
  const char* state=s.state==input::State::Receiving?"RECEIVING":s.state==input::State::Fault?"FAULT":s.state==input::State::Disabled?"DISABLED":"WAITING";
  return Value::Object{{"endpoint",std::string(endpoint.begin(),endpoint.end())},{"state",state},{"layout",s.session.layout_id},{"channels",s.session.channels},{"rate",s.session.sample_rate},{"format","FP64 PLANAR"},{"sourceSession",uuid},{"streamEpoch",Value::u64(s.lastEpoch)},{"blockSequence",Value::u64(s.lastSequence)},{"firstFrame",Value::u64(s.lastFirstFrame)},{"sourceQpc",Value::u64(s.sourceQpc)},{"lastBlockQpc",Value::u64(s.lastBlockQpc)},{"musicSessionRevision",Value::u64(s.block.music_session_revision)},{"sourceGeneration",std::move(generations)},{"sourceRate",std::move(rates)},{"sourceState",std::move(states)},{"localToken",Value::u64(s.generation)},{"received",Value::u64(s.received)},{"consumed",Value::u64(s.consumed)},{"discardedInactive",Value::u64(s.discarded)},{"gaps",Value::u64(s.gaps)},{"faults",Value::u64(s.faults)},{"queued",s.queued},{"producerPid",s.producerPid},{"peaks",std::move(peaks)},{"maximumPeaks",std::move(maximumPeaks)},{"maximumReceiveGapQpc",Value::u64(s.maximumReceiveGapQpc)},{"maximumSourceAgeQpc",Value::u64(s.maximumSourceAgeQpc)}};};
 auto m=in.telemetry(input::Domain::Music),c=in.telemetry(input::Domain::Cinema);v["inputs"]=Value::Object{{"music",encode(m,music)},{"cinema",encode(c,cinema)}};auto selected=selection.program()?c:m;const auto name=selection.program()?"CINEMA":"MUSIC";
 v["inputState"]=selected.state==input::State::Receiving?std::string(name)+" FP64 / 192 kHz":selected.state==input::State::Fault?std::string(name)+" INPUT FAULT / LOCAL RESTART REQUIRED":std::string("WAITING ")+name;
 if(selected.state!=input::State::Receiving){for(const auto key:{"musicPeaks","cinemaPeaks","crossoverPeaks"})for(auto& p:v[key].asArray())p=0.0;for(auto& p:v["matrix"]["peaks"].asArray())p=0.0;for(auto& p:v["headroom"]["peaks"].asArray())p=0.0;}
 return v;
}
Value telemetrySnapshot(const control::ControlPlane& controls,const control::AudioScheduler& scheduler,bool signal,const input::Stage2Ingress* stage2){
 auto v=controls.snapshot();auto t=scheduler.telemetry();v["pid"]=unsigned(GetCurrentProcessId());v["armed"]=false;v["hardwareGate"]="BLOCKED: Stage04 FAIL REALTIME";v["offlineSignal"]=signal;v["inputState"]=signal?"INTERNAL OFFLINE SIGNAL":"WAITING FOR STAGE2";
 if(stage2){auto s=stage2->telemetry();v["inputState"]=input::Stage2Ingress::stateText(s.state);v["blockSize"]=1024;
  LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
  const bool fresh=s.state==input::State::Receiving&&s.lastBlockQpc&&uint64_t(now.QuadPart)>=s.lastBlockQpc&&uint64_t(now.QuadPart)-s.lastBlockQpc<uint64_t(frequency.QuadPart)/5;
  v["stage2"]=Value::Object{{"received",Value::u64(s.received)},{"consumed",Value::u64(s.consumed)},{"emptyPolls",Value::u64(s.empty)},{"faults",Value::u64(s.faults)},{"generation",Value::u64(s.generation)},{"queued",s.queued},{"producerPid",s.producerPid},{"validityMask",s.validityMask},{"lastBlockQpc",Value::u64(s.lastBlockQpc)},{"metersFresh",fresh}};
  v["stage2"]["releaseFailures"]=Value::u64(s.releaseFailures);
  std::string uuid;constexpr char hex[]="0123456789abcdef";for(auto b:s.sourceSession){uuid+=hex[b>>4];uuid+=hex[b&15];}
  v["stage2"]["transportVersion"]=2;v["stage2"]["sourceSession"]=uuid;
  v["stage2"]["gaps"]=Value::u64(s.gaps);v["stage2"]["sourceQpc"]=Value::u64(s.sourceQpc);
  v["stage2"]["sourceDiscontinuityMask"]=s.sourceDiscontinuityMask;
  v["stage2"]["sourceFlaggedBlocks"]=Value::u64(s.sourceFlaggedBlocks);
  v["stage2"]["blockSequence"]=Value::u64(s.lastSequence);v["stage2"]["firstFrame"]=Value::u64(s.lastFirstFrame);v["stage2"]["streamEpoch"]=Value::u64(s.lastEpoch);
  if(!fresh){for(const auto key:{"musicPeaks","cinemaPeaks","crossoverPeaks"})for(auto& peak:v[key].asArray())peak=0.0;for(auto& peak:v["matrix"]["peaks"].asArray())peak=0.0;for(auto& peak:v["headroom"]["peaks"].asArray())peak=0.0;if(s.state==input::State::Receiving)v["inputState"]="STAGE2 CONNECTED / WAITING FOR BLOCK";}
 }
 v["scheduler"]=Value::Object{{"callbackWallUs",t.callbackUs},{"maximumCallbackWallUs",t.maxCallbackUs},{"schedulerLatenessUs",t.latenessUs},{"maximumSchedulerLatenessUs",t.maxLatenessUs},{"callbackWallDeadlineMisses",Value::u64(t.misses)},{"faults",t.faults},{"maximumInputIntervalUs",t.maximumInputIntervalUs}};
 // Honest stopped-domain state. No estimate or meters are fabricated for unopened DACs.
 v["domains"]=Value::Array{Value::Object{{"id","DAC A"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}},Value::Object{{"id","DAC B"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}}};return v;
}
}

