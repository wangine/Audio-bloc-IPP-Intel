#include "DspAutoLink.h"
#include <windows.h>
namespace entropy::stage {
namespace {uint64_t tick(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}uint64_t freq(){LARGE_INTEGER q;QueryPerformanceFrequency(&q);return q.QuadPart;}}
DspAutoLinkState DspAutoLink::poll(uint64_t now,DspStageRuntime& runtime,DspLocalDemand& local,std::string& error,bool localMonitor){
 using namespace entropy::link;
 if(connected_){
  auto before=localMonitor_?local.step(now):DspStepResult::Idle;auto result=runtime.step(now);auto after=localMonitor_?local.step(tick()):DspStepResult::Idle;
  bool alive=true;if(now-heartbeat_>=freq()/4){runtime.heartbeat(tick());alive=runtime.peersAlive(tick());heartbeat_=now;}
  if(before==DspStepResult::Fault || result==DspStepResult::Fault || after==DspStepResult::Fault || !alive){
   std::memcpy(failedSession_.data(),owner_.session.session_id,16);local.close();runtime.stop();connected_=false;retry_=now+freq();error="StageBus peer lost or faulted; waiting for a fresh Source session";return DspAutoLinkState::Fault;
  }error.clear();return DspAutoLinkState::Receiving;
 }
 if(now<retry_)return DspAutoLinkState::WaitingSource;retry_=now+freq();
 StageLinkResponse query{};if(!client_.transact(makeRequest(StageLinkAction::Query),query,500,error))return DspAutoLinkState::WaitingSource;
 if(query.status!=StageLinkStatus::Ok){error="Source unavailable or faulted; a fresh Source session is required";return DspAutoLinkState::WaitingSource;}
 if(!std::memcmp(failedSession_.data(),query.session.session_id,16)){error="Waiting for fresh Source UUID";return DspAutoLinkState::WaitingSource;}
 auto attach=makeRequest(StageLinkAction::Attach);std::memcpy(attach.expectedSession,query.session.session_id,16);
 if(!client_.transact(attach,owner_,1500,error))return DspAutoLinkState::WaitingSource;
 if(owner_.status!=StageLinkStatus::Ok){error=owner_.status==StageLinkStatus::Busy?"Source link is owned by another DSP instance":"Source refused ownership handoff; waiting";return DspAutoLinkState::WaitingSource;}
 localMonitor_=localMonitor;
 if(!runtime.attach(owner_.session,error) || (localMonitor_ && !local.attach(runtime.outputSession(),owner_.program,owner_.controlRevision,error))){
  std::memcpy(failedSession_.data(),owner_.session.session_id,16);local.close();runtime.stop();return DspAutoLinkState::Fault;
 }
 StageLinkResponse confirmed{};if(!client_.transact(makeRequest(StageLinkAction::Query),confirmed,500,error) || confirmed.status!=StageLinkStatus::Ok || std::memcmp(confirmed.session.session_id,owner_.session.session_id,16) || confirmed.sourcePid!=owner_.sourcePid){
  std::memcpy(failedSession_.data(),owner_.session.session_id,16);local.close();runtime.stop();error="Source identity changed during attachment";return DspAutoLinkState::Fault;
 }
 runtime.heartbeat(tick());if(localMonitor_)local.step(tick());heartbeat_=tick();connected_=true;error.clear();return DspAutoLinkState::Receiving;
}
bool DspAutoLink::finish(DspStageRuntime& runtime,DspLocalDemand& local,std::string& error){
 using namespace entropy::link;if(!connected_){local.close();runtime.stop();return true;}
 local.beginQuiesce();const auto deadline=tick()+freq();
 while(local.pending() || runtime.pending()){
  runtime.heartbeat(tick());if((localMonitor_ && local.step(tick())==DspStepResult::Fault) || runtime.step(tick())==DspStepResult::Fault || tick()>deadline){error="Cannot confirm drained DISARM; Source lease retained until fresh session";local.close();runtime.stop();connected_=false;return false;}Sleep(1);
 }
 runtime.heartbeat(tick());auto release=makeRequest(StageLinkAction::Release);std::memcpy(release.lease,owner_.lease,16);std::memcpy(release.expectedSession,owner_.session.session_id,16);StageLinkResponse reply{};
 const bool ok=client_.transact(release,reply,500,error) && reply.status==StageLinkStatus::Ok;
 if(!ok && error.empty())error="Source refused release; a fresh Source session is required";
 local.close();runtime.stop();connected_=false;retry_=0;return ok;
}
}
