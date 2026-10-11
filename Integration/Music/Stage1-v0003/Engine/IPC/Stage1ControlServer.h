#pragma once
#include "EngineLifecycle.h"
#include <thread>
#include <windows.h>
namespace entropy::music {
class Stage1ControlServer {
    std::thread worker_;
    HANDLE stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);

  public:
    ~Stage1ControlServer();
    bool start(const std::wstring &, EngineLifecycle &);
    void stop();
};
} // namespace entropy::music
