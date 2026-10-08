#include "TelemetryServer.h"
#include <windows.h>
namespace entropy::ipc {
Value telemetrySnapshot(const control::ControlPlane& controls,const control::AudioScheduler& scheduler,bool signal){
 auto v=controls.snapshot();auto t=scheduler.telemetry();v["pid"]=unsigned(GetCurrentProcessId());v["armed"]=false;v["hardwareGate"]="BLOCKED: Stage04 FAIL REALTIME";v["offlineSignal"]=signal;v["inputState"]=signal?"INTERNAL OFFLINE SIGNAL":"WAITING FOR STAGE2";
 v["scheduler"]=Value::Object{{"callbackWallUs",t.callbackUs},{"maximumCallbackWallUs",t.maxCallbackUs},{"schedulerLatenessUs",t.latenessUs},{"maximumSchedulerLatenessUs",t.maxLatenessUs},{"callbackWallDeadlineMisses",Value::u64(t.misses)},{"faults",t.faults}};
 // Honest stopped-domain state. No estimate or meters are fabricated for unopened DACs.
 v["domains"]=Value::Array{Value::Object{{"id","DAC A"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}},Value::Object{{"id","DAC B"},{"state","DISARMED"},{"clockValid",false},{"fifoFrames",0},{"xruns",Value::u64(0)}}};return v;
}
}
