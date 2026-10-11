#pragma once
#include "IPC/Json.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace entropy::control {
// Non-audio owner of preparation, profiles and device lifecycle. Admission
// never runs a handler. Only DISARM may overtake queued semantic commands;
// it invalidates ARM generations, without discarding any profile operation.
class LifecycleExecutor final {
public:
 enum class State { Accepted, Preparing, Arming, Running, Stopping, Disarmed, Fault };
 using Handler=std::function<ipc::Value(const ipc::Value&,uint64_t)>;
 LifecycleExecutor(Handler,std::function<void()> closeGate,std::function<void()> tick,std::function<void()> finalize={},std::function<bool()> faultProbe={});
 ~LifecycleExecutor();
 ipc::Value admit(const ipc::Value&,bool cancelArm=false);
 ipc::Value results(const ipc::Value&) const;
 uint64_t generation() const noexcept {return generation_.load(std::memory_order_acquire);}
 bool current(uint64_t g) const noexcept {return g==generation();}
 bool activate(uint64_t,const std::function<bool()>&);
 void state(State s) noexcept {state_=s;}
 void stateIfCurrent(uint64_t,State);
 void transition(State expected,State next) noexcept {state_.compare_exchange_strong(expected,next);}
 const char* stateText() const noexcept;
 void stop();
private:
 struct Entry {uint64_t id,generation;ipc::Value request,reply;bool complete=false,closesGate=false;};
 void run();
 Handler handler_;std::function<void()> closeGate_,tick_,finalize_;std::function<bool()> faultProbe_;
 mutable std::mutex queueMutex_;std::mutex activationMutex_;
 std::condition_variable wake_;std::deque<std::shared_ptr<Entry>> queue_,history_;
 std::atomic<uint64_t> generation_{1};std::atomic<State> state_{State::Disarmed};
 uint64_t next_=0;bool stopping_=false;std::thread owner_;
};
}
