#pragma once
#include "StageLinkProtocol.h"
#include "StageLinkSecurity.h"
namespace entropy::link {
class StageLinkClient {public:explicit StageLinkClient(std::wstring endpoint=endpointName()):endpoint_(std::move(endpoint)){}bool transact(const StageLinkRequest&,StageLinkResponse&,uint32_t timeoutMs,std::string&);private:std::wstring endpoint_;};
}
