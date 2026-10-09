#pragma once
#include "Codec.h"
#include <windows.h>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
namespace entropy::ipc {
// Dedicated non-realtime transport owner. Handler never runs on an audio owner.
class CommandServer final {
public:
    using Handler=std::function<Frame(const Frame&)>;
    using Disconnect=std::function<void()>;
    static constexpr DWORD kControlLeaseMs=5000;
    CommandServer()=default;
    ~CommandServer(){stop();}
    CommandServer(const CommandServer&)=delete;
    bool start(const std::wstring& endpoint,Handler,Disconnect,std::string& error);
    void stop() noexcept;
    void requestStopAfterReply() noexcept{stopAfterReply_=true;}
    bool shutdownReplyComplete() const noexcept{return replyComplete_.load(std::memory_order_acquire);}
    static std::wstring pipePath(const std::wstring& endpoint);
private:
    void worker() noexcept;
    HANDLE pipe_=INVALID_HANDLE_VALUE,stop_=nullptr;
    std::thread thread_;Handler handler_;Disconnect disconnect_;
    std::atomic<bool> stopAfterReply_{false},replyComplete_{false};
};
}
