#pragma once
#include <windows.h>
#include <avrt.h>
#include <cstdint>
#include <filesystem>

namespace entropy::stage04_test {
// Test-driver instrumentation only; no production audio code calls these APIs.
class SchedulingProbe final {
public:
    struct Snapshot {
        PERFORMANCE_DATA data{};
        uint64_t cpu100ns=0;
        int64_t beginQpc=0,endQpc=0;
        DWORD error=0;
    };
    explicit SchedulingProbe(bool useMmcss);
    ~SchedulingProbe();
    SchedulingProbe(const SchedulingProbe&)=delete;
    Snapshot capture() noexcept;
    bool ready() const noexcept {return dispatchError_==ERROR_SUCCESS && (!requestedMmcss_ || (mmcss_ && priorityOk_));}
    bool selfTest(); // Intentional Sleep is outside callback/audit scopes.
    void report(const std::filesystem::path&) const;
private:
    HANDLE profiling_=nullptr,mmcss_=nullptr;
    DWORD dispatchError_=0,mmcssError_=0,priorityError_=0,taskIndex_=0;
    DWORD threadId_=0;bool requestedMmcss_=false,priorityOk_=false,selfTestPassed_=false;
    uint64_t positiveSwitches_=0,positiveCycles_=0,reads_=0,readErrors_=0;
};
}
