#pragma once
#include <windows.h>
#include <atomic>

namespace entropy::control {
// Control/lifecycle thread only. No use from an audio callback.
class ConsoleShutdown {
public:
    void reset() noexcept { requested_=false;complete_=false; }
    bool requested() const noexcept { return requested_.load(std::memory_order_acquire); }
    void complete() noexcept { complete_.store(true,std::memory_order_release); }
    BOOL handle(DWORD event) noexcept {
        if(event!=CTRL_C_EVENT&&event!=CTRL_BREAK_EVENT&&event!=CTRL_CLOSE_EVENT)return FALSE;
        requested_.store(true,std::memory_order_release);
        if(event==CTRL_CLOSE_EVENT){
            // Windows terminates the process when a CLOSE handler returns.
            // Keep it alive while the control thread drains/releases ingress
            // and closes egress, within the usual 5-second OS close budget.
            const auto deadline=GetTickCount64()+4500;
            while(!complete_.load(std::memory_order_acquire)&&GetTickCount64()<deadline)Sleep(5);
        }
        return TRUE;
    }
private:
    std::atomic<bool> requested_{false},complete_{false};
};
}
