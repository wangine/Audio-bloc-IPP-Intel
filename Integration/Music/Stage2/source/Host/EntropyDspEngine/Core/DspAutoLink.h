#pragma once
#include "DspLocalDemand.h"
#include "StageLinkClient.h"
namespace entropy::stage {
enum class DspAutoLinkState {WaitingSource,Receiving,Fault};
class DspAutoLink {
public:
 explicit DspAutoLink(std::wstring endpoint=entropy::publish::sourceEndpointName()):endpoint_(std::move(endpoint)){}
 DspAutoLinkState poll(uint64_t,DspStageRuntime&,std::string&);
 bool finish(DspStageRuntime&,std::string&);
 // Source-compatible legacy fixtures only. The local demand is never stepped.
 DspAutoLinkState poll(uint64_t now,DspStageRuntime& runtime,DspLocalDemand&,std::string& error,bool=true){return poll(now,runtime,error);}
 bool finish(DspStageRuntime& runtime,DspLocalDemand&,std::string& error){return finish(runtime,error);}
 bool connected() const noexcept{return connected_;}
 const auto& owner() const noexcept{return owner_;}
private:
 std::wstring endpoint_;entropy::link::StageLinkResponse owner_{};
 uint64_t retry_=0,heartbeat_=0;bool connected_=false;
};
}
