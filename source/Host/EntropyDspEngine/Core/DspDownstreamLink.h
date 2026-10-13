#pragma once
#include "DspAutoLink.h"
#include "StageLinkServer.h"
namespace entropy::stage {
// Read-only V1 discovery compatibility; external ownership requests are refused.
class DspDownstreamLink {
public:
 explicit DspDownstreamLink(std::wstring endpoint):server_(std::move(endpoint)){}
 ~DspDownstreamLink(){stop();}
 bool start(std::string& error){return server_.start(error);}
 void service(DspAutoLink&,DspStageRuntime&,DspLocalDemand&,std::string&);
 bool external() const noexcept {return false;}
 void stop(){server_.stop();}
private:
 entropy::link::StageLinkServer server_;
};
}
