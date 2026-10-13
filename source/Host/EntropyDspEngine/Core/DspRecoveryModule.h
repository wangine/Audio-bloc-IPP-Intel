#pragma once
#include <entropy/module_abi.h>
#include <entropy/dynamic_recovery_abi.h>
#include <filesystem>
#include <string>
#include <windows.h>

namespace entropy::stage {
EntropyDynamicRecoveryConfigV1 defaultRecoverySettings() noexcept;
bool validRecoverySettings(const EntropyDynamicRecoveryConfigV1&) noexcept;

struct RecoverySnapshot {
    uint64_t loaded = 0, active = 0, revision = 0, latencySamples = 0;
    EntropyDynamicRecoveryConfigV1 settings{};
};

class DspRecoveryModule {
public:
    DspRecoveryModule() = default;
    DspRecoveryModule(const DspRecoveryModule&) = delete;
    DspRecoveryModule& operator=(const DspRecoveryModule&) = delete;
    ~DspRecoveryModule() { close(); }
    bool prepare(const std::filesystem::path&, const EntropyDynamicRecoveryConfigV1&, std::string&);
    bool apply(const EntropyDynamicRecoveryConfigV1&, uint64_t revision = 1) noexcept;
    bool process(const double* left, const double* right, double* outLeft, double* outRight) noexcept;
    bool reset() noexcept;
    void close() noexcept;
    bool loaded() const noexcept { return handle_ != nullptr; }
    bool active() const noexcept { return snapshot_.active != 0; }
    const RecoverySnapshot& snapshot() const noexcept { return snapshot_; }
private:
    HMODULE library_ = nullptr;
    EntropyModuleHandle handle_ = nullptr;
    EntropyModuleApiV1 api_{};
    RecoverySnapshot snapshot_{};
};
}
