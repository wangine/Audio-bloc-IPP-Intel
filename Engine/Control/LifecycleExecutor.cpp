#include "LifecycleExecutor.h"
#include <chrono>
#include <objbase.h>
namespace entropy::control {
using J=ipc::Value;
LifecycleExecutor::LifecycleExecutor(Handler h,std::function<void()> gate,std::function<void()> tick,std::function<void()> finalize,std::function<bool()> faultProbe)
 :handler_(std::move(h)),closeGate_(std::move(gate)),tick_(std::move(tick)),finalize_(std::move(finalize)),faultProbe_(std::move(faultProbe)),owner_([this]{run();}){}
LifecycleExecutor::~LifecycleExecutor(){stop();}
void LifecycleExecutor::stop(){
 {std::lock_guard activation(activationMutex_);++generation_;closeGate_();}
 {std::lock_guard lock(queueMutex_);stopping_=true;}wake_.notify_one();if(owner_.joinable())owner_.join();
}
J LifecycleExecutor::admit(const J& request,bool cancelArm){
 const auto begin=std::chrono::steady_clock::now();double gateUs=0;
 if(cancelArm){const auto a=std::chrono::steady_clock::now();
  std::lock_guard activation(activationMutex_);++generation_;closeGate_();state_=State::Stopping;
  gateUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-a).count();
 }
 std::lock_guard lock(queueMutex_);
 if(stopping_)throw std::runtime_error("Lifecycle executor stopping");
 // A stopped gate is never conditional on available queue capacity.
 if(queue_.size()>=256&&!cancelArm)throw std::runtime_error("Preparation queue full; request not accepted");
 while(history_.size()>=512&&history_.front()->complete)history_.pop_front();
 auto entry=std::make_shared<Entry>();entry->id=++next_;entry->generation=generation();entry->request=request;entry->closesGate=cancelArm;
 if(request.at("action").asString()=="disarm")queue_.push_front(entry);else queue_.push_back(entry);
 history_.push_back(entry);wake_.notify_one();
 return J::Object{{"status","ACCEPTED"},{"commandId",J::u64(entry->id)},{"generation",J::u64(entry->generation)},
  {"admissionUs",std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()},{"gateUs",gateUs}};
}
J LifecycleExecutor::results(const J& ids) const {
 J::Array results;std::lock_guard lock(queueMutex_);
 for(const auto& id:ids.asArray()){
  J result=J::Object{{"commandId",id},{"complete",false}};bool found=false;
  for(const auto& e:history_)if(e->id==id.asU64()){found=true;result["complete"]=e->complete;if(e->complete)result["reply"]=e->reply;break;}
  if(!found){result["complete"]=true;result["reply"]=J::Object{{"ok",false},{"status","RESULT_EXPIRED"},{"error","Completion unknown: command identity expired; inspect Core state before retrying"}};}
  results.push_back(std::move(result));
 }
 return J::Object{{"results",std::move(results)}};
}
bool LifecycleExecutor::activate(uint64_t g,const std::function<bool()>& start){
 std::lock_guard lock(activationMutex_);if(!current(g))return false;return start();
}
void LifecycleExecutor::stateIfCurrent(uint64_t g,State state){std::lock_guard lock(activationMutex_);if(current(g))state_=state;}
const char* LifecycleExecutor::stateText() const noexcept {
 switch(state_.load()){
 case State::Accepted:return "ACCEPTED";case State::Preparing:return "PREPARING";case State::Arming:return "ARMING";
 case State::Running:return "RUNNING";case State::Stopping:return "STOPPING";case State::Fault:return "FAULT";default:return "DISARMED";}
}
void LifecycleExecutor::run(){
 const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 while(true){
  std::shared_ptr<Entry> work;
  {std::unique_lock lock(queueMutex_);wake_.wait_for(lock,std::chrono::milliseconds(50),[&]{return stopping_||!queue_.empty();});
   if(stopping_)break;if(!queue_.empty()){work=queue_.front();queue_.pop_front();}}
  if(work){
   const auto previousState=state_.load();J reply;const auto begin=std::chrono::steady_clock::now();
   try{if(work->request.at("action").asString()=="arm"&&!current(work->generation))
     reply=J::Object{{"ok",false},{"status","CANCELED"},{"error","ARM canceled by DISARM"}};
    else reply=J::Object{{"ok",true},{"data",handler_(work->request,work->generation)}};
   }catch(const std::exception& e){reply=J::Object{{"ok",false},{"error",e.what()}};
    if(work->request.at("action").asString()=="arm"&&!current(work->generation))reply["status"]="CANCELED";
    else if(work->closesGate){
     // Admission already closed publication. Even an invalid profile/device
     // request must finish teardown; never leave a stopped gate in STOPPING.
     try{handler_(J::Object{{"action","disarm"}},work->generation);stateIfCurrent(work->generation,State::Disarmed);}
     catch(const std::exception& teardown){reply["teardownError"]=teardown.what();stateIfCurrent(work->generation,State::Fault);}
    }else stateIfCurrent(work->generation,work->request.at("action").asString()=="arm"?State::Fault:previousState);
   }
   reply["executionUs"]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
   try{tick_();}catch(...){stateIfCurrent(work->generation,State::Fault);}
   {std::lock_guard lock(queueMutex_);work->reply=std::move(reply);work->complete=true;}
  }
  if(!work)try{tick_();}catch(...){state_=State::Fault;}
  // Fault teardown belongs to this non-audio owner. Invalidate queued ARM
  // generations and finish closing the owners; their original fault record
  // remains available until a subsequent explicit ARM prepares a new session.
  try{if(faultProbe_&&faultProbe_()){
   uint64_t faultGeneration;
   {std::lock_guard activation(activationMutex_);faultGeneration=++generation_;closeGate_();state_=State::Stopping;}
   handler_(J::Object{{"action","disarm"}},faultGeneration);
   stateIfCurrent(faultGeneration,State::Disarmed);
  }}catch(...){state_=State::Fault;}
 }
 if(finalize_)finalize_();
 if(SUCCEEDED(com))CoUninitialize();
}
}
