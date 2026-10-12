#pragma once
#include <windows.h>
#include <chrono>
// Owner-thread scheduling only. Short Sleep waits can round to a scheduler
// tick; keep the existing decoder deadlines without requesting a global tick.
class CinemaDeadlineWait {
 HANDLE timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
public:
 CinemaDeadlineWait()=default;
 ~CinemaDeadlineWait(){if(timer_)CloseHandle(timer_);}
 bool valid() const noexcept{return timer_!=nullptr;}
 CinemaDeadlineWait(const CinemaDeadlineWait&)=delete;
 CinemaDeadlineWait& operator=(const CinemaDeadlineWait&)=delete;
 bool forDuration(std::chrono::nanoseconds duration) noexcept {
  if(duration.count()<=0)return true;
  LARGE_INTEGER due{};due.QuadPart=-(duration.count()+99)/100;
  return SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE)&&WaitForSingleObject(timer_,INFINITE)==WAIT_OBJECT_0;
 }
 bool until(std::chrono::steady_clock::time_point deadline) noexcept {
  return forDuration(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()));
 }
};
