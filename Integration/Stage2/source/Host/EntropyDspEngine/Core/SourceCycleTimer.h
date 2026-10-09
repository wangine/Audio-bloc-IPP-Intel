#pragma once
#include <windows.h>
#include <chrono>
namespace entropy::host
{
// EngineMain R10 timer extracted unchanged; worker waits only, never a sink callback.
class SourceCycleTimer final
{
  public:
    SourceCycleTimer()
    {
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_MODIFY_STATE | SYNCHRONIZE);
        if (timer_ == nullptr)
            timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }

    ~SourceCycleTimer()
    {
        if (timer_ != nullptr)
            CloseHandle(timer_);
    }

    bool valid() const noexcept { return timer_ != nullptr; }

    bool waitUntil(std::chrono::steady_clock::time_point deadline) noexcept
    {
        if (timer_ == nullptr)
            return false;
        const auto now = std::chrono::steady_clock::now();
        if (deadline <= now)
            return true;

        auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - now);
        // Relative waitable-timer due time is expressed in negative 100-ns units.
        auto ticks100ns = (remaining.count() + 99) / 100;
        if (ticks100ns < 1)
            ticks100ns = 1;
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(ticks100ns);
        if (!SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE))
            return false;
        return WaitForSingleObject(timer_, INFINITE) == WAIT_OBJECT_0;
    }

  private:
    HANDLE timer_ = nullptr;
};
} // namespace entropy::host
