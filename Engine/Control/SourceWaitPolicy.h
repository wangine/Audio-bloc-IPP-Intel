#pragma once
#include "Engine/Egress/LiveSinkWorkers.h"
#include <atomic>
namespace entropy::control {
// Lifecycle-owner service; DISARM admission may cancel intent concurrently.
// Waiting stops native playback, never fabricates Graph/source PCM.
class SourceWaitPolicy final {
public:
 enum class Result {None,Held,Resumed,Canceled,Failed,Faulted};
 explicit SourceWaitPolicy(LiveSinkWorkers& sinks):sinks_(sinks){}
 void requestArm(bool wait=false) noexcept {heldReceiveQpc_=0;muted_=wait;requested_=true;}
 void cancel() noexcept {requested_=false;muted_=false;}
 bool requested() const noexcept {return requested_.load(std::memory_order_acquire);}
 bool muted() const noexcept {return requested()&&muted_.load(std::memory_order_acquire);}
 uint64_t waits() const noexcept {return waits_;}
 uint64_t resumes() const noexcept {return resumes_;}
 unsigned lastSourceCause() const noexcept {return lastCause_;}
 template<class Resume>Result service(bool receiving,uint64_t receiveQpc,bool policyReady,Resume resume) {
  Result result=Result::None;
  if(requested()&&!muted()&&sinks_.sourceWaitFault()) {
   lastCause_=sinks_.sourceWaitFaults();sinks_.stop();
   if(!requested())return Result::Canceled;
   if(!sinks_.sourceWaitFault()){cancel();return Result::Faulted;}
   heldReceiveQpc_=receiveQpc;muted_=true;++waits_;return Result::Held;
  }
  if(muted()&&receiving&&receiveQpc&&receiveQpc!=heldReceiveQpc_&&policyReady) {
   if(sinks_.faulted()&&!sinks_.sourceWaitFault()){cancel();sinks_.stop();return Result::Faulted;}
   bool started=false;
   try{started=resume();}catch(...){cancel();sinks_.stop();throw;}
   if(!requested()){sinks_.stop();return Result::Canceled;}
   if(!started){cancel();sinks_.stop();return Result::Failed;}
   muted_=false;++resumes_;return Result::Resumed;
  }
  return result;
 }
private:
 LiveSinkWorkers& sinks_;
 std::atomic<bool> requested_{false},muted_{false};
 uint64_t waits_=0,resumes_=0,heldReceiveQpc_=0;unsigned lastCause_=0;
};
}
