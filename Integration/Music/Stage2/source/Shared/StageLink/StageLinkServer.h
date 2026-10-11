#pragma once
#include "StageLinkSecurity.h"
#include "StageLinkProtocol.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
namespace entropy::link {
class StageLinkServer {public:explicit StageLinkServer(std::wstring name=endpointName()):endpoint_(std::move(name)){}~StageLinkServer(){stop();}bool start(std::string&);bool tryTake(StageLinkRequest&,uint32_t& verifiedPid);void complete(const StageLinkResponse&);void stop();private:void worker();std::wstring endpoint_;HANDLE pipe_=INVALID_HANDLE_VALUE,stop_=nullptr;std::thread thread_;std::mutex mutex_;std::condition_variable cv_;bool pending_=false,taken_=false,replied_=false;uint32_t pid_=0;StageLinkRequest request_{};StageLinkResponse response_{};};
}
