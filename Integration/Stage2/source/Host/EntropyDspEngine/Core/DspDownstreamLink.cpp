#include "DspDownstreamLink.h"
namespace entropy::stage {
void DspDownstreamLink::service(DspAutoLink& link,DspStageRuntime& runtime,DspLocalDemand&,std::string& error){
 using namespace entropy::link;
 StageLinkRequest request{};uint32_t verified=0;if(!server_.tryTake(request,verified))return;
 StageLinkResponse response{};response.sourcePid=GetCurrentProcessId();std::memcpy(response.nonce,request.nonce,16);
 response.status=link.connected()?StageLinkStatus::Ok:StageLinkStatus::Waiting;
 if(request.action==StageLinkAction::Attach || request.action==StageLinkAction::Release){
  // V1 output belongs permanently to local nominal demand. Subscriber lifecycle
  // is handled by StagePublish V2 and never drains/releases the upstream lease.
  response.status=StageLinkStatus::Refused;
  error="V1 external takeover disabled; subscribe to StagePublish V2";
 }
 response.session=runtime.outputSession();response.program=link.owner().program;response.controlRevision=link.owner().controlRevision;
 server_.complete(response);
}
}
