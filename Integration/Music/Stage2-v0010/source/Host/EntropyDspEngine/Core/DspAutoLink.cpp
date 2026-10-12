#include "DspAutoLink.h"
#include <windows.h>
namespace entropy::stage {
namespace {uint64_t tick(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}uint64_t freq(){LARGE_INTEGER q;QueryPerformanceFrequency(&q);return q.QuadPart;}}
DspAutoLinkState DspAutoLink::poll(uint64_t now,DspStageRuntime& runtime,std::string& error){
 if(connected_){
  if(now-heartbeat_>=freq()/4){runtime.heartbeat(tick());heartbeat_=now;}
  if(runtime.step(now)==DspStepResult::Fault){
   connected_=false;retry_=now+freq()/10;
   error="Passive Source subscription lost/gapped/invalid; local history retired, reconnecting";return DspAutoLinkState::Fault;
  }
  error.clear();return DspAutoLinkState::Receiving;
 }
 if(now<retry_)return DspAutoLinkState::WaitingSource;retry_=now+freq()/10;
 if(!runtime.attachPassive(endpoint_,error))return DspAutoLinkState::WaitingSource;
 owner_={};owner_.session=runtime.outputSession();owner_.sourcePid=owner_.session.producer_pid;
 heartbeat_=now;connected_=true;error.clear();return DspAutoLinkState::Receiving;
}
bool DspAutoLink::finish(DspStageRuntime& runtime,std::string& error){
 runtime.stop();connected_=false;retry_=heartbeat_=0;error.clear();return true;
}
}
