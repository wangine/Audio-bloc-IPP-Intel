#pragma once
#include "StageLinkClient.h"
#include "DspLocalDemand.h"
namespace entropy::stage {
enum class DspAutoLinkState {WaitingSource,Receiving,Fault};
class DspAutoLink {
public:
 explicit DspAutoLink(std::wstring endpoint=entropy::link::endpointName()):client_(std::move(endpoint)){}
 DspAutoLinkState poll(uint64_t,DspStageRuntime&,DspLocalDemand&,std::string&,bool localMonitor=true);
 bool finish(DspStageRuntime&,DspLocalDemand&,std::string&);
 bool connected() const noexcept {return connected_;}
 const auto& owner() const noexcept {return owner_;}
private:
 entropy::link::StageLinkClient client_;entropy::link::StageLinkResponse owner_{};
 std::array<uint8_t,16> failedSession_{};uint64_t retry_=0,heartbeat_=0;bool connected_=false,localMonitor_=true;
};
}
