#include "DspController.h"
#include "DspSessionDiscovery.h"
#include "SourceCycleTimer.h"
#include "ProAudioThreadScope.h"
#include <cstring>
#include <chrono>
#include <functional>
namespace entropy::stage {
namespace {uint64_t tick(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}}
bool DspController::configureEq(const std::filesystem::path& root,const EqSettings& settings,std::string& error){
 if(thread_.joinable()||!validEqSettings(settings)){error="Configure valid EQ settings before worker starts";return false;}
 eqModuleRoot_=root;eqConfigured_=true;eqSchemaReady_=false;eqRequestRevision_=0;eqSubmittedRevision_=0;uint64_t serial=0;return requestEq(settings,serial);
}
bool DspController::requestEq(const EqSettings& settings,uint64_t& revision) noexcept {
 if(!eqConfigured_||!validEqSettings(settings))return false;
 static_assert(sizeof(EqSettings)%8==0);eqRequestRevision_.fetch_add(1,std::memory_order_acq_rel);
 for(unsigned i=0;i<eqRequestValues_.size();i++){uint64_t word;std::memcpy(&word,reinterpret_cast<const char*>(&settings)+i*8,8);eqRequestValues_[i].store(word,std::memory_order_relaxed);}
 revision=eqRequestRevision_.fetch_add(1,std::memory_order_release)/2+1;return true;
}
bool DspController::readEqRequest(EqSettings& settings,uint64_t& revision) const noexcept {
 for(unsigned attempt=0;attempt<16;attempt++){auto begin=eqRequestRevision_.load(std::memory_order_acquire);if(begin&1)continue;
  for(unsigned i=0;i<eqRequestValues_.size();i++){auto word=eqRequestValues_[i].load(std::memory_order_relaxed);std::memcpy(reinterpret_cast<char*>(&settings)+i*8,&word,8);}
  std::atomic_thread_fence(std::memory_order_acquire);if(begin==eqRequestRevision_.load(std::memory_order_acquire)){revision=begin/2;return revision!=0;}
 }return false;
}
bool DspController::prepareEqWorker(std::string& error){
 if(!eqConfigured_)return true;EqSettings settings{};uint64_t revision=0;
 if(!readEqRequest(settings,revision)||!runtime_.prepareEq(eqModuleRoot_,settings,error,revision))return false;
 eqSubmittedRevision_=revision;
 // Same SHA-pinned DLL on restart: never rewrite a schema already visible to UI.
 if(!eqSchemaReady_.load(std::memory_order_acquire)){eqSchema_=runtime_.eqSchema();eqSchemaReady_.store(true,std::memory_order_release);}return true;
}
bool DspController::adoptEqRequest() noexcept {
 if(!eqConfigured_||runtime_.pending()||runtime_.eqCommandPending())return true;
 EqSettings settings{};uint64_t revision=0;if(!readEqRequest(settings,revision)||revision==eqSubmittedRevision_)return true;
 if(!runtime_.applyEq(settings,revision))return false;eqSubmittedRevision_=revision;return true;
}
bool DspController::configureAligner(const std::filesystem::path& root,std::string& error){
 if(thread_.joinable()){error="Configure LRC before worker starts";return false;}alignerModuleRoot_=root;alignerConfigured_=true;alignmentAppliedRevision_=0;alignmentRequestRevision_=0;return true;
}
bool DspController::prepareAlignerWorker(std::string& error){
 if(!alignerConfigured_)return true;
 if(!runtime_.prepareAligner(alignerModuleRoot_,error))return false;
 // A fresh worker starts in bypass. Cancel queued commands from the old session,
 // retaining monotonic acknowledgement serials for existing UI observers.
 const auto serial=alignmentRequestRevision_.load(std::memory_order_acquire)/2;
 const bool cancelled=serial!=alignmentAppliedRevision_;
 alignmentAppliedRevision_=serial;
 runtime_.acknowledgeAlignment(serial,cancelled);
 return true;
}
bool DspController::requestAlignment(bool align) noexcept {
 if(!alignerConfigured_)return false;auto s=snapshot();if(!s.diag.alignment.loaded)return false;
 if(alignmentRequestRevision_.load(std::memory_order_acquire)/2!=s.diag.alignment.commandSerial)return false;
 if(align){CenterSettings requested;uint64_t revision=0;if(!s.diag.alignment.valid||!readCenterRequest(requested,revision)||revision!=s.diag.center.revision)return false;}
 alignmentRequestRevision_.fetch_add(1,std::memory_order_acq_rel);
 alignmentRequestValues_[0].store(align,std::memory_order_relaxed);alignmentRequestValues_[1].store(s.diag.center.revision,std::memory_order_relaxed);alignmentRequestValues_[2].store(s.diag.alignment.detected[2],std::memory_order_relaxed);
 alignmentRequestRevision_.fetch_add(1,std::memory_order_release);return true;
}
bool DspController::adoptAlignmentRequest() noexcept {
 if(!alignerConfigured_||runtime_.pending())return true;
 for(unsigned attempt=0;attempt<16;attempt++){
  auto begin=alignmentRequestRevision_.load(std::memory_order_acquire);if(begin&1)continue;if(begin/2==alignmentAppliedRevision_)return true;
  std::array<uint64_t,3> v{};for(unsigned i=0;i<3;i++)v[i]=alignmentRequestValues_[i].load(std::memory_order_relaxed);std::atomic_thread_fence(std::memory_order_acquire);
  if(begin!=alignmentRequestRevision_.load(std::memory_order_acquire))continue;
  auto result=runtime_.applyAlignment(v[0]!=0,v[1],v[2]);if(result==AlignmentApplyResult::Busy)return true;
  alignmentAppliedRevision_=begin/2;runtime_.acknowledgeAlignment(alignmentAppliedRevision_,result!=AlignmentApplyResult::Applied);
  return result!=AlignmentApplyResult::NativeFault;
 }return true;
}
bool DspController::configureCenter(const std::filesystem::path& root,const CenterSettings& settings,std::string& error){
 if(thread_.joinable()){error="Configure module root before DSP starts";return false;}
 if(!validCenterSettings(settings)){error="Invalid Center settings";return false;}
 centerModuleRoot_=root;centerConfigured_=true;centerAppliedRevision_=0;centerRequestRevision_=0;
 return requestCenter(settings);
}
bool DspController::requestCenter(const CenterSettings& settings) noexcept {
 if(!centerConfigured_ || !validCenterSettings(settings))return false;
 centerRequestRevision_.fetch_add(1,std::memory_order_acq_rel);
 for(unsigned i=0;i<16;i++){uint64_t word;std::memcpy(&word,&settings.values[i],8);centerRequestValues_[i].store(word,std::memory_order_relaxed);}
 centerRequestRevision_.fetch_add(1,std::memory_order_release);return true;
}
bool DspController::readCenterRequest(CenterSettings& settings,uint64_t& revision) const noexcept {
 for(unsigned attempt=0;attempt<16;attempt++){
  const auto begin=centerRequestRevision_.load(std::memory_order_acquire);if(begin&1)continue;
  for(unsigned i=0;i<16;i++){auto word=centerRequestValues_[i].load(std::memory_order_relaxed);std::memcpy(&settings.values[i],&word,8);}
  std::atomic_thread_fence(std::memory_order_acquire);
  if(begin==centerRequestRevision_.load(std::memory_order_acquire)){revision=begin/2;return revision!=0;}
 }return false;
}
bool DspController::prepareCenterWorker(std::string& error){
 if(!centerConfigured_)return true;CenterSettings settings;uint64_t revision=0;
 if(!readCenterRequest(settings,revision)){error="Center control snapshot unavailable";return false;}
 if(!runtime_.prepareCenter(centerModuleRoot_,settings,error))return false;
 if(!runtime_.applyCenter(settings,revision)){error="Center initial settings rejected";runtime_.closeCenter();return false;}
 centerAppliedRevision_=revision;return true;
}
bool DspController::adoptCenterRequest() noexcept {
 if(!centerConfigured_ || runtime_.pending())return true;
 CenterSettings settings;uint64_t revision=0;
 if(!readCenterRequest(settings,revision) || revision==centerAppliedRevision_)return true;
 if(!runtime_.applyCenter(settings,revision))return false;
 centerAppliedRevision_=revision;return true;
}
bool DspController::startCapture(const std::filesystem::path& path,uint64_t blocks,std::string& error){if(thread_.joinable()){error="STOP before preparing bounded capture";return false;}if(!recorder_.start(path,blocks)){error="Cannot prepare bounded capture (1..11250 blocks)";return false;}runtime_.setRecorder(&recorder_);return true;}
bool DspController::start(const std::filesystem::path& root,const std::filesystem::path& source,std::string& error){
 if(thread_.joinable()){error="DSP already started; STOP before restart";return false;}
 StageBusSessionHeader s{};if(!DspSessionDiscovery::readExternal(source,s,error)){publish(DspStateCode::Fault,error.c_str());return false;}
 if(attachmentProbe_)attachmentProbe_(probeContext_);
 if(!runtime_.attach(s,error)){publish(DspStateCode::Fault,error.c_str());return false;}
 StageBusSessionHeader confirmed{};
 if(!DspSessionDiscovery::readExternal(source,confirmed,error) || std::memcmp(&s,&confirmed,sizeof(s))){runtime_.stop();error="Source discovery changed during attachment; select current EXTERNAL session";publish(DspStateCode::Fault,error.c_str());return false;}
 if(!DspSessionDiscovery::write(root/"state"/"StageBusSession.json",runtime_.outputSession(),"EXTERNAL",error)){runtime_.stop();publish(DspStateCode::Fault,error.c_str());return false;}
 stopping_=false;publish(DspStateCode::WaitingDownstream);
 try{thread_=std::thread(&DspController::worker,this);}catch(...){runtime_.stop();error="Cannot start DSP worker";publish(DspStateCode::Fault,error.c_str());return false;}return true;
}
bool DspController::startAutomatic(const std::filesystem::path& root,std::string& error,std::wstring endpoint){
 if(thread_.joinable()){error="DSP already started";return false;}
 auto ownerName=std::wstring(L"Local\\Entropy.Stage2.Automatic.Owner.V1");
 if(endpoint!=entropy::link::endpointName())ownerName+=L".Fixture."+std::to_wstring(std::hash<std::wstring>{}(endpoint));
 automaticOwner_=CreateMutexW(nullptr,FALSE,ownerName.c_str());
 if(!automaticOwner_ || GetLastError()==ERROR_ALREADY_EXISTS){if(automaticOwner_)CloseHandle(automaticOwner_);automaticOwner_=nullptr;error="Another DSP instance owns the automatic link";publish(DspStateCode::Stopped,error.c_str());return false;}
 publishEndpoint_=endpoint==entropy::link::endpointName()?entropy::publish::endpointName():endpoint+L".StagePublish.V2";
 publisher_=std::make_unique<entropy::publish::Publisher>();
 if(!publisher_->start(publishEndpoint_,error)){publisher_.reset();CloseHandle(automaticOwner_);automaticOwner_=nullptr;return false;}
 runtime_.setPublisher(publisher_.get());
 runtimeRoot_=root;downstreamLink_=std::make_unique<DspDownstreamLink>(endpoint+L".Downstream.V1");if(!downstreamLink_->start(error)){runtime_.setPublisher(nullptr);publisher_.reset();CloseHandle(automaticOwner_);automaticOwner_=nullptr;downstreamLink_.reset();return false;}autoLink_=std::make_unique<DspAutoLink>(std::move(endpoint));stopping_=false;publish(DspStateCode::WaitingSource);
 try{thread_=std::thread(&DspController::automaticWorker,this);}catch(...){runtime_.setPublisher(nullptr);publisher_.reset();downstreamLink_.reset();CloseHandle(automaticOwner_);automaticOwner_=nullptr;autoLink_.reset();error="Cannot start automatic DSP worker";publish(DspStateCode::Stopped,error.c_str());return false;}return true;
}
void DspController::stop() noexcept {stopping_=true;if(thread_.joinable())thread_.join();downstreamLink_.reset();runtime_.stop();runtime_.setPublisher(nullptr);publisher_.reset();recorder_.stop();autoLink_.reset();if(automaticOwner_){CloseHandle(automaticOwner_);automaticOwner_=nullptr;}auto final=snapshot();publish(final.state==DspStateCode::Fault?DspStateCode::Fault:DspStateCode::Stopped,final.error);}
void DspController::publish(DspStateCode state,const char* error) noexcept {
 DspUiSnapshot local{};local.state=state;local.diag=runtime_.diagnostics();local.diag.monitorScheduleRebases=localDemand_.scheduleRebases();runtime_.summarizeLatency(local.diag);local.outputSession=runtime_.outputSession();std::strncpy(local.error,error,sizeof(local.error)-1);
 snapshotRevision_.fetch_add(1,std::memory_order_acq_rel);
 for(size_t i=0;i<snapshotWords_.size();i++){uint64_t word;std::memcpy(&word,reinterpret_cast<const char*>(&local)+i*8,8);snapshotWords_[i].store(word,std::memory_order_relaxed);}
 snapshotRevision_.fetch_add(1,std::memory_order_release);
}
DspUiSnapshot DspController::snapshot() const noexcept {
 static_assert(std::atomic<uint64_t>::is_always_lock_free);DspUiSnapshot local{};
 for(unsigned attempt=0;attempt<16;attempt++){auto begin=snapshotRevision_.load(std::memory_order_acquire);if(begin&1)continue;for(size_t i=0;i<snapshotWords_.size();i++){auto word=snapshotWords_[i].load(std::memory_order_relaxed);std::memcpy(reinterpret_cast<char*>(&local)+i*8,&word,8);}std::atomic_thread_fence(std::memory_order_acquire);if(begin==snapshotRevision_.load(std::memory_order_acquire))return local;}
 local={};std::strcpy(local.error,"Snapshot busy; retry next refresh");return local;
}
void DspController::automaticWorker() noexcept {
 entropy::host::ProAudioThreadScope realtime;
 struct CenterLifetime {DspController& owner;~CenterLifetime(){auto last=owner.snapshot();owner.runtime_.closeCenter();owner.runtime_.closeAligner();owner.runtime_.closeEq();owner.publish(last.state==DspStateCode::Fault?DspStateCode::Fault:DspStateCode::Stopped,last.error);}} centerLifetime{*this};
 try{
  entropy::host::SourceCycleTimer timer;std::string error;bool advertised=false;std::array<uint8_t,16> advertisedSession{};uint64_t refresh=0;
  if(!timer.valid()){publish(DspStateCode::Fault,"Worker timer unavailable");return;}
  if(!prepareCenterWorker(error)||!prepareAlignerWorker(error)||!prepareEqWorker(error)){publish(DspStateCode::Fault,error.c_str());return;}
  while(!stopping_){
   if(!adoptCenterRequest()||!adoptEqRequest()||!adoptAlignmentRequest()){publish(DspStateCode::Fault,"Native DSP command rejected");break;}
   downstreamLink_->service(*autoLink_,runtime_,localDemand_,error);
   publisher_->service();
   const auto state=autoLink_->poll(tick(),runtime_,localDemand_,error,true);
   const bool receiving=state==DspAutoLinkState::Receiving;
   if(receiving && (!advertised || std::memcmp(advertisedSession.data(),runtime_.outputSession().session_id,16))){
    if(!DspSessionDiscovery::write(runtimeRoot_/"state"/"StageBusSession.json",runtime_.outputSession(),"LOCAL_MONITOR",error,publishEndpoint_)){publish(DspStateCode::Fault,error.c_str());break;}advertised=true;std::memcpy(advertisedSession.data(),runtime_.outputSession().session_id,16);
   }else if(!receiving && advertised){std::string ignored;DspSessionDiscovery::write(runtimeRoot_/"state"/"StageBusSession.json",runtime_.outputSession(),"INACTIVE",ignored,publishEndpoint_);advertised=false;}
   if(!runtime_.pollEqTelemetry()){publish(DspStateCode::Fault,"EQ control telemetry failed");break;}
   const auto now=tick();LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
   if(now-refresh>=static_cast<uint64_t>(frequency.QuadPart)/50){publish(receiving && runtime_.diagnostics().blocksOut?DspStateCode::Running:state==DspAutoLinkState::Fault?DspStateCode::Fault:DspStateCode::WaitingSource,error.c_str());refresh=now;}
   if(!timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::microseconds(receiving?100:10000))){publish(DspStateCode::Fault,"Worker scheduling wait failed");break;}
  }
  if(!autoLink_->finish(runtime_,localDemand_,error))publish(DspStateCode::Fault,error.c_str());
  std::string ignored;DspSessionDiscovery::write(runtimeRoot_/"state"/"StageBusSession.json",runtime_.outputSession(),"INACTIVE",ignored,publishEndpoint_);
 }catch(const std::exception& e){publish(DspStateCode::Fault,e.what());localDemand_.close();runtime_.stop();}

}
void DspController::worker() noexcept {
 entropy::host::ProAudioThreadScope realtime;
 struct CenterLifetime {DspController& owner;~CenterLifetime(){auto last=owner.snapshot();owner.runtime_.closeCenter();owner.runtime_.closeAligner();owner.runtime_.closeEq();owner.publish(last.state==DspStateCode::Fault?DspStateCode::Fault:DspStateCode::Stopped,last.error);}} centerLifetime{*this};
 std::string centerError;if(!prepareCenterWorker(centerError)||!prepareAlignerWorker(centerError)||!prepareEqWorker(centerError)){publish(DspStateCode::Fault,centerError.c_str());return;}
 entropy::host::SourceCycleTimer timer;if(!timer.valid()){publish(DspStateCode::Fault,"Worker timer unavailable");return;}
 uint64_t heartbeat=0,refresh=0;const auto handshakeStart=tick();const auto freq=runtime_.outputSession().qpc_frequency;
 while(!stopping_){
  if(!adoptCenterRequest()||!adoptEqRequest()||!adoptAlignmentRequest()){publish(DspStateCode::Fault,"Native DSP command rejected");break;}
  auto now=tick();
  if(now-heartbeat>=freq/4){runtime_.heartbeat(now);if(!runtime_.peersAlive(now)){publish(DspStateCode::Fault,"StageBus peer lost or faulted; new UUID required");break;}heartbeat=now;}
  if(!runtime_.downstreamReady() && now-handshakeStart>freq*5){publish(DspStateCode::Fault,"Downstream handshake timeout (5000 ms); new UUID required");break;}
  auto result=runtime_.step(now);
  if(!runtime_.pollEqTelemetry()){publish(DspStateCode::Fault,"EQ control telemetry failed");break;}
  if(result==DspStepResult::Fault){publish(DspStateCode::Fault,"StageBus protocol/deadline fault; new UUID required");break;}
  if(now-refresh>=freq/50 || result==DspStepResult::Published){publish(runtime_.diagnostics().blocksOut?DspStateCode::Running:DspStateCode::WaitingDownstream);refresh=now;}
  // Scheduling wait only on IPC worker; this is not a second programme clock.
  if(result==DspStepResult::Idle && !timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::microseconds(100))){publish(DspStateCode::Fault,"Worker scheduling wait failed");break;}
 }

}
}
