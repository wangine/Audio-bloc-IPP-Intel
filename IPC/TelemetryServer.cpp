#include "TelemetryServer.h"
#include <windows.h>
namespace entropy::ipc {
Value telemetrySnapshot(const control::ControlPlane& controls,const control::AudioScheduler& scheduler,bool signal,const input::Stage2Ingress* stage2){
 auto v=controls.snapshot();auto t=scheduler.telemetry();v["pid"]=unsigned(GetCurrentProcessId());v["armed"]=false;v["hardwareGate"]="BLOCKED: Stage04 FAIL REALTIME";v["offlineSignal"]=signal;v["inputState"]=signal?"INTERNAL OFFLINE SIGNAL":"WAITING FOR STAGE2";
 if(stage2){auto s=stage2->telemetry();v["inputState"]=input::Stage2Ingress::stateText(s.state);v["blockSize"]=1024;
  LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
  const bool fresh=s.state==input::State::Receiving&&s.lastBlockQpc&&uint64_t(now.QuadPart)>=s.lastBlockQpc&&uint64_t(now.QuadPart)-s.lastBlockQpc<uint64_t(frequency.QuadPart)/5;
  v["stage2"]=Value::Object{{"received",Value::u64(s.received)},{"consumed",Value::u64(s.consumed)},{"emptyPolls",Value::u64(s.empty)},{"faults",Value::u64(s.faults)},{"generation",Value::u64(s.generation)},{"queued",s.queued},{"producerPid",s.producerPid},{"validityMask",s.validityMask},{"lastBlockQpc",Value::u64(s.lastBlockQpc)},{"metersFresh",fresh}};
  if(!fresh){for(const auto key:{"musicPeaks","cinemaPeaks","crossoverPeaks"})for(auto& peak:v[key].asArray())peak=0.0;for(auto& peak:v["matrix"]["peaks"].asArray())peak=0.0;for(auto& peak:v["headroom"]["peaks"].asArray())peak=0.0;if(s.state==input::State::Receiving)v["inputState"]="STAGE2 CONNECTED / WAITING FOR BLOCK";}
 }
 v["scheduler"]=Value::Object{{"callbackWallUs",t.callbackUs},{"maximumCallbackWallUs",t.maxCallbackUs},{"schedulerLatenessUs",t.latenessUs},{"maximumSchedulerLatenessUs",t.maxLatenessUs},{"callbackWallDeadlineMisses",Value::u64(t.misses)},{"faults",t.faults}};
 // Honest stopped-domain state. No estimate or meters are fabricated for unopened DACs.
 v["domains"]=Value::Array{Value::Object{{"id","DAC A"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}},Value::Object{{"id","DAC B"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}}};return v;
}
}
