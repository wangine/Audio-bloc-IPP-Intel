#pragma once
#include "stage1_control_v1.h"
#include <filesystem>
#include <memory>
#include <string>
namespace entropy::music {
class EngineLifecycle {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    EngineLifecycle();
    ~EngineLifecycle();
    bool start(const std::filesystem::path &config, const std::filesystem::path &providers,
               const std::wstring &publish, std::string &error);
    void stop();
    Stage1ControlResponse control(const Stage1ControlRequest &);
    Stage1ControlResponse snapshot();
};
} // namespace entropy::music
