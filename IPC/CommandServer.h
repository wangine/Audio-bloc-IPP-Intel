#pragma once
#include "Codec.h"
#include <windows.h>
#include <functional>
#include <string>
#include <thread>
namespace entropy::ipc {
// Dedicated non-realtime transport owner. Handler never runs on an audio owner.
class CommandServer final {
public:
    using Handler=std::function<Frame(const Frame&)>;
    using Disconnect=std::function<void()>;
    CommandServer()=default;
    ~CommandServer(){stop();}
    CommandServer(const CommandServer&)=delete;
    bool start(const std::wstring& endpoint,Handler,Disconnect,std::string& error);
    void stop() noexcept;
    static std::wstring pipePath(const std::wstring& endpoint);
private:
    void worker() noexcept;
    HANDLE pipe_=INVALID_HANDLE_VALUE,stop_=nullptr;
    std::thread thread_;Handler handler_;Disconnect disconnect_;
};
}
