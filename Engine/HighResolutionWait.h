#pragma once
#include <windows.h>
#include <chrono>

namespace entropy::control {
// Owner-thread scheduling only. Construct before processing; never use inside DSP.
class HighResolutionWait {
 HANDLE timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
public:
 HighResolutionWait()=default;
 ~HighResolutionWait(){if(timer_)CloseHandle(timer_);}
 bool valid() const noexcept{return timer_!=nullptr;}
 HighResolutionWait(const HighResolutionWait&)=delete;
 void forDuration(std::chrono::nanoseconds duration) noexcept {
  if(duration.count()<=0)return;
  LARGE_INTEGER due{};due.QuadPart=-(duration.count()+99)/100;
  if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer_,INFINITE);
 }
 void until(std::chrono::steady_clock::time_point deadline) noexcept {
  forDuration(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()));
 }
};
}
