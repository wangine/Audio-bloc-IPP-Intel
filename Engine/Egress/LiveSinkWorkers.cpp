#include "LiveSinkWorkers.h"
#include "RendererLive/DomainCore.h"
#include "RendererLive/WasapiOutput.h"
#include <windows.h>
#include <avrt.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
namespace entropy::control {
namespace {
struct Event {
 HANDLE value=nullptr;
 explicit Event(bool manual=false){value=CreateEventW(nullptr,manual?TRUE:FALSE,FALSE,nullptr);if(!value)throw std::runtime_error("Renderer event creation failed");}
 ~Event(){if(value)CloseHandle(value);}Event(const Event&)=delete;Event& operator=(const Event&)=delete;
};
struct Priority {
 HANDLE value=nullptr;
 explicit Priority(AVRT_PRIORITY priority){DWORD task=0;value=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);if(!value)throw std::runtime_error("MMCSS registration failed");if(!AvSetMmThreadPriority(value,priority)){AvRevertMmThreadCharacteristics(value);value=nullptr;throw std::runtime_error("MMCSS priority failed");}}
 ~Priority(){if(value)AvRevertMmThreadCharacteristics(value);}
};
// Retains the existing injectable sink contract, not its old implementation.
class PreparedWasapiAdapter final:public egress_host::Sink {
 live::WasapiOutput output_;void* cancel_=nullptr;EntropyDacSinkConfig config_{};unsigned state_=ENTROPY_DAC_SINK_CLOSED;
public:
 void setCancelEvent(void* c) noexcept override{cancel_=c;}
 EntropyResult open(const EntropyDacSinkConfig& c) override {
  config_=c;output_.open({c.endpoint_id_utf8,c.channels,c.output_format,c.requested_buffer_frames});state_=ENTROPY_DAC_SINK_OPEN;return ENTROPY_OK;
 }
 EntropyResult start() noexcept override{if(!output_.start())return ENTROPY_ERROR_PROCESSING;state_=ENTROPY_DAC_SINK_RUNNING;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& request) noexcept override {
  if(output_.wait(cancel_,request.timeout_ms)!=live::WasapiOutput::Wait::Ready)return ENTROPY_ERROR_PROCESSING;
  const auto o=output_.observation();request.frames=o.quantum;request.bytes=o.quantum*config_.channels*(config_.output_format?4:3);request.device_position_frames=o.position;request.device_qpc_position=o.qpc100ns;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& block) noexcept override {
  block.accepted_frames=0;if(!output_.submit(block.data,block.frames,block.byte_count))return ENTROPY_ERROR_PROCESSING;block.accepted_frames=block.frames;return ENTROPY_OK;
 }
 EntropyDacSinkStatus status() const noexcept override {
  const auto o=output_.observation();EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.state=state_;s.last_driver_result=o.lastResult;s.applied_revision=config_.revision;
  s.sample_rate=192000;s.channels=config_.channels;s.output_format=config_.output_format;s.buffer_frames=o.quantum;s.block_alignment_bytes=config_.channels*(config_.output_format?4:3);
  s.submitted_frames=o.submitted;s.rendered_buffers=o.quantum?o.submitted/o.quantum:0;s.xrun_count=o.timeouts;s.timeout_count=o.timeouts;s.device_position_frames=o.position;s.device_qpc_position=o.qpc100ns;return s;
 }
 egress_host::SinkTiming timing() const noexcept override {const auto o=output_.observation();return {o.clockFrequency,uint64_t(o.quantum)*10000000/192000,o.clockValid,double(o.position),false};}
 void stop() noexcept override{output_.stop();if(state_!=ENTROPY_DAC_SINK_CLOSED)state_=ENTROPY_DAC_SINK_OPEN;}
 void close() noexcept override{output_.stop();state_=ENTROPY_DAC_SINK_CLOSED;}
};
}
struct LiveSinkWorkers::Impl {
 struct Domain {
  LiveDomain config;live::DomainCore core;Event work,ready;
  std::thread sinkOwner,prepareOwner;
  std::atomic<bool> stopped{false},failureClaimed{false},open{false},running{false},done{true},mmcss{false};
  std::atomic<unsigned> flags{0},quantum{0},target{8192},sinkState{ENTROPY_DAC_SINK_CLOSED};
  std::array<char,384> detail{};
  std::atomic<uint64_t> callbacks{0},submitted{0},position{0},qpc{0},clockFrequency{0},timeouts{0},xruns{0},misses{0},mappingVersion{0};
  std::atomic<double> callbackUs{0},maxCallbackUs{0},prepareUs{0},maxPrepareUs{0},waitUs{0},firstGraph{0},endGraph{0};
  std::atomic<int> driverResult{0};std::atomic<bool> clockValid{false};
 };
 struct Session {std::array<Domain,2> domains;unsigned configuredMask=0;std::atomic<unsigned> failedMask{0};std::atomic<bool> stopped{false},sourceHold{false};};
 Factory factory;std::array<Event,2> cancel{Event(true),Event(true)};
 std::unique_ptr<Session> session;
 std::atomic<Session*> requested{nullptr};Session* attached=nullptr;
 std::atomic<uint64_t> request{0},ack{0};
 std::atomic<bool> graphRunning{false},enabled{false},fault{false};
 std::atomic<unsigned> runningMask{0};bool prepared=false;uint64_t legacyFrame=0;double ticksPerMicrosecond=1;
 explicit Impl(Factory f):factory(std::move(f)){LARGE_INTEGER hz{};QueryPerformanceFrequency(&hz);if(hz.QuadPart<=0)throw std::runtime_error("QPC frequency unavailable");ticksPerMicrosecond=double(hz.QuadPart)/1e6;}
 ~Impl(){stop();}
 double ticks() const noexcept {LARGE_INTEGER t{};QueryPerformanceCounter(&t);return double(t.QuadPart);}
 bool detached() const noexcept {return !graphRunning.load(std::memory_order_acquire)||ack.load(std::memory_order_acquire)==request.load(std::memory_order_acquire);}
 bool canceled(const Session& s,unsigned slot) const noexcept {return s.stopped.load(std::memory_order_acquire)||s.domains[slot].stopped.load(std::memory_order_acquire)||WaitForSingleObject(cancel[slot].value,0)==WAIT_OBJECT_0;}
 void fail(Session& s,unsigned slot,unsigned reason,const char* message) noexcept {
  auto& d=s.domains[slot];bool expected=false;
  if(d.failureClaimed.compare_exchange_strong(expected,true,std::memory_order_acq_rel)){
   size_t n=0;if(message)while(n+1<d.detail.size()&&message[n]){d.detail[n]=message[n];++n;}d.detail[n]=0;
   d.flags.store(reason?reason:egress_host::Device,std::memory_order_release);
   const unsigned mask=s.failedMask.fetch_or(1u<<slot,std::memory_order_acq_rel)|(1u<<slot);
   if((mask&s.configuredMask)==s.configuredMask)fault.store(true,std::memory_order_release);
  }
  d.stopped.store(true,std::memory_order_release);d.running=false;runningMask.fetch_and(~(1u<<slot));
  SetEvent(cancel[slot].value);SetEvent(d.work.value);SetEvent(d.ready.value);
 }
 void globalFail(unsigned reason,bool source) noexcept {
  auto* s=attached;if(!s)return;if(source)s->sourceHold=true;
  for(unsigned i=0;i<2;++i)if(s->domains[i].config.enabled)fail(*s,i,reason,source?"Upstream source unavailable":"Graph/policy failure");
 }
 void closeGate() noexcept {enabled.store(false,std::memory_order_release);for(auto& e:cancel)SetEvent(e.value);}
 void stop() noexcept {
  closeGate();requested.store(nullptr,std::memory_order_release);request.fetch_add(1,std::memory_order_release);
  if(session){session->stopped=true;for(auto& d:session->domains){d.stopped=true;SetEvent(d.work.value);SetEvent(d.ready.value);}
   for(auto& d:session->domains){if(d.sinkOwner.joinable())d.sinkOwner.join();if(d.prepareOwner.joinable())d.prepareOwner.join();}}
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(!detached()&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  runningMask=0;prepared=false;
 }
 bool prepare(const std::array<LiveDomain,2>& configs){
  stop();if(!detached())return false;
  unsigned count=0,allowed=0;std::array<bool,16> used{};
  for(unsigned i=0;i<2;++i){const auto& c=configs[i];if(!c.enabled)continue;++count;
   if(!c.channels||c.channels>16||c.format>1||c.endpoint.empty()||c.endpoint.size()>=ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES||c.logicalId!=(i?"DAC_B":"DAC_A"))return false;
   if(i&&configs[0].enabled&&c.endpoint==configs[0].endpoint)return false;
   for(unsigned k=0;k<c.channels;++k){if(c.lanes[k]>15||used[c.lanes[k]])return false;used[c.lanes[k]]=true;}if(c.liveAllowed)++allowed;
  }
  if(!count||!allowed)return false;
  auto next=std::make_unique<Session>();fault=false;runningMask=0;
  for(unsigned i=0;i<2;++i){next->domains[i].config=configs[i];if(configs[i].enabled)next->configuredMask|=1u<<i;}
  for(unsigned i=0;i<2;++i){auto& d=next->domains[i];if(!d.config.enabled)continue;
   if(!d.config.liveAllowed){fail(*next,i,egress_host::Device,d.config.liveError.c_str());continue;}
   uint64_t seed=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||!seed)return false;
   if(!d.core.prepare(d.config.channels,d.config.format,seed,false))return false;
  }
  session=std::move(next);prepared=true;return true;
 }
 bool start(){
  if(!prepared||enabled.load()||!session)return false;auto& s=*session;
  s.stopped=false;legacyFrame=0;enabled=true;prepared=false;
  for(unsigned i=0;i<2;++i)if(s.domains[i].config.enabled&&s.domains[i].config.liveAllowed)ResetEvent(cancel[i].value);
  requested.store(&s,std::memory_order_release);request.fetch_add(1,std::memory_order_release);
  try{for(unsigned i=0;i<2;++i){auto& d=s.domains[i];if(d.config.enabled&&d.config.liveAllowed){d.done=false;d.sinkOwner=std::thread([this,&s,i]{runSink(s,i);});}}return true;}
  catch(...){stop();return false;}
 }
 void boundary() noexcept {graphRunning=true;const auto serial=request.load(std::memory_order_acquire);if(serial!=ack.load(std::memory_order_relaxed)){attached=requested.load(std::memory_order_acquire);ack.store(serial,std::memory_order_release);}}
 void publish(const double* const* output,unsigned frames,bool policy,egress_host::GraphInterval interval) noexcept {
  if(!graphRunning.load(std::memory_order_acquire)){attached=requested.load(std::memory_order_acquire);ack.store(request.load(std::memory_order_acquire),std::memory_order_release);}
  auto* s=attached;if(!s||!enabled.load(std::memory_order_acquire)||s->stopped.load())return;
  if(!policy||!output||!frames||frames>4096||interval.frames!=frames){globalFail(egress_host::Input,false);return;}
  for(unsigned i=0;i<2;++i){auto& d=s->domains[i];if(!d.config.enabled||!d.config.liveAllowed||d.stopped.load(std::memory_order_acquire))continue;
   // A domain admits its first source frame only after its own device has
   // negotiated successfully. Slow open is not an active-stream backlog.
   // Once admitted, AudioQueue enforces every subsequent frame in order;
   // there is no queue retirement, skipped block or phase jump during playback.
   if(!d.open.load(std::memory_order_acquire))continue;
   std::array<const double*,16> selected{};for(unsigned c=0;c<d.config.channels;++c)selected[c]=output[d.config.lanes[c]];
   if(!d.core.input.push(selected.data(),frames,interval.firstGraphFrame,interval.epoch)){
    const bool full=d.core.input.readable()+frames>65536;if(full)++d.xruns;
    fail(*s,i,full?egress_host::Overflow:egress_host::Input,full?"Elastic input queue full; no drop/overwrite":"Invalid or discontinuous Graph PCM");
   }else SetEvent(d.work.value);
  }
 }
 void runPreparation(Session& s,unsigned slot) noexcept {
  auto& d=s.domains[slot];
  try{
   Priority priority(AVRT_PRIORITY_HIGH);const HANDLE waits[]={cancel[slot].value,d.work.value};
   while(!canceled(s,slot)&&d.core.input.readable()<d.core.primeFrames())WaitForMultipleObjects(2,waits,FALSE,100);
   while(!canceled(s,slot)){
    const double begin=ticks();const auto result=d.core.produce();const double cost=(ticks()-begin)/ticksPerMicrosecond;
    if(result==live::DomainCore::Production::Fault){fail(s,slot,egress_host::Pcm,"ASRC/PCM preparation failed; no partial flush");break;}
    if(result==live::DomainCore::Production::Made){d.prepareUs=cost;d.maxPrepareUs=(std::max)(d.maxPrepareUs.load(),cost);SetEvent(d.ready.value);continue;}
    WaitForMultipleObjects(2,waits,FALSE,2);
   }
  }catch(const std::exception& e){fail(s,slot,egress_host::Device,e.what());}catch(...){fail(s,slot,egress_host::Device,"Preparation owner failed");}
 }
 void runSink(Session& s,unsigned slot) noexcept {
  auto& d=s.domains[slot];std::unique_ptr<egress_host::Sink> sink;
  auto cache=[&]{if(!sink)return;const auto t=sink->status();const auto clock=sink->timing();d.submitted=t.submitted_frames;d.position=t.device_position_frames;d.qpc=t.device_qpc_position;d.driverResult=t.last_driver_result;d.timeouts=t.timeout_count;d.sinkState=t.state;d.clockFrequency=clock.clockFrequency;d.clockValid=clock.freshClock;};
  try{
   Priority priority(AVRT_PRIORITY_CRITICAL);d.mmcss=true;
   sink=factory?factory(slot):std::make_unique<PreparedWasapiAdapter>();if(!sink)throw std::runtime_error("Sink factory returned null");sink->setCancelEvent(cancel[slot].value);
   EntropyDacSinkConfig config{};config.struct_size=sizeof(config);config.backend=ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;config.sample_rate=192000;config.channels=d.config.channels;config.output_format=d.config.format;config.requested_buffer_frames=1024;config.revision=1;
   std::memcpy(config.endpoint_id_utf8,d.config.endpoint.c_str(),d.config.endpoint.size()+1);
   if(sink->open(config)!=ENTROPY_OK)throw std::runtime_error("Exact saved sink open failed");
   const auto status=sink->status();if(!d.core.setQuantum(status.buffer_frames))throw std::runtime_error("Negotiated buffer exceeds preallocated 4096-frame maximum");
   d.quantum=status.buffer_frames;d.target=d.core.target();d.open=true;cache();
   if(canceled(s,slot))throw std::runtime_error("Start canceled");
   d.prepareOwner=std::thread([this,&s,slot]{runPreparation(s,slot);});
   const auto deadline=GetTickCount64()+5000;const HANDLE readyWaits[]={cancel[slot].value,d.ready.value};
   while(!canceled(s,slot)&&d.core.ready.size()<3){if(GetTickCount64()>deadline)throw std::runtime_error("No source prefill within five seconds");WaitForMultipleObjects(2,readyWaits,FALSE,100);}
   auto submit=[&]{
    const auto* packet=d.core.ready.beginRead();if(!packet){++d.xruns;throw std::runtime_error("Prepared PCM queue depleted; no replay/drop concealment");}
    const auto frames=packet->frames;const double first=packet->sourceBegin,end=packet->sourceEnd;
    EntropyDacSinkPcmBlock block{};block.struct_size=sizeof(block);block.frames=frames;block.data=packet->bytes.data();block.byte_count=d.core.bytesPerPacket();
    if(sink->submit(block)!=ENTROPY_OK||block.accepted_frames!=frames)throw std::runtime_error("Device rejected prepared PCM; cursor not advanced");
    if(!d.core.submitted())throw std::runtime_error("Submitted packet accounting failed");
    d.mappingVersion.fetch_add(1,std::memory_order_acq_rel);d.firstGraph=double(d.core.input.baseFrame())+first;d.endGraph=double(d.core.input.baseFrame())+end;d.mappingVersion.fetch_add(1,std::memory_order_release);
    ++d.callbacks;SetEvent(d.work.value);
   };
   if(!canceled(s,slot)){
    submit();if(sink->start()!=ENTROPY_OK)throw std::runtime_error("Sink Start failed");d.running=true;runningMask.fetch_or(1u<<slot,std::memory_order_release);cache();
   }
   while(!canceled(s,slot)){
    EntropyDacSinkBufferRequest request{};request.struct_size=sizeof(request);request.timeout_ms=100;
    const double waitBegin=ticks();const auto result=sink->wait(request);const double begin=ticks();d.waitUs=(begin-waitBegin)/ticksPerMicrosecond;
    if(canceled(s,slot))break;
    if(result!=ENTROPY_OK)throw std::runtime_error("WASAPI event wait failed");
    if(request.frames!=d.quantum.load()||request.bytes!=d.core.bytesPerPacket())throw std::runtime_error("Sink changed negotiated packet contract");
    submit();const double cost=(ticks()-begin)/ticksPerMicrosecond;d.callbackUs=cost;d.maxCallbackUs=(std::max)(d.maxCallbackUs.load(),cost);
    if(cost>double(request.frames)*1e6/192000)++d.misses;cache();
   }
  }catch(const std::exception& e){cache();if(!canceled(s,slot))fail(s,slot,egress_host::Device,e.what());}
   catch(...){cache();if(!canceled(s,slot))fail(s,slot,egress_host::Device,"Sink owner failed");}
  if(sink){sink->stop();cache();sink->close();}
  d.stopped=true;SetEvent(d.work.value);SetEvent(d.ready.value);d.running=false;d.open=false;d.done=true;d.sinkState=ENTROPY_DAC_SINK_CLOSED;runningMask.fetch_and(~(1u<<slot));
 }
 ipc::Value snapshot() const {
  using J=ipc::Value;J::Array out;if(!session)return out;
  for(unsigned i=0;i<2;++i){const auto& d=session->domains[i];const unsigned flags=d.flags.load(std::memory_order_acquire);const auto pcm=d.core.pcmTelemetry();
   const char* state=!d.config.enabled?"DISARMED":flags?"FAULT":d.running.load()?"RUNNING":d.open.load()?"PREROLL":enabled.load()?"ARMING":"DISARMED";
   out.emplace_back(J::Object{{"id",i?"DAC B":"DAC A"},{"configured",d.config.enabled},{"channels",d.config.channels},{"rate",192000},{"format",d.config.format},{"endpoint",d.config.endpoint},{"offlineFixture",false},{"physicalSinkOpen",d.open.load()},{"state",state},{"sinkState",d.sinkState.load()},{"driverResult",d.driverResult.load()},{"sinkFaults",flags?unsigned(ENTROPY_DAC_SINK_FAULT_DRIVER):0u},{"faults",flags},{"bufferFrames",d.quantum.load()},{"submittedFrames",J::u64(d.submitted.load())},{"devicePositionFrames",J::u64(d.position.load())},{"callbacks",J::u64(d.callbacks.load())},{"mmcssRegistered",d.mmcss.load()},{"guardian","EXACT BYPASS"}});
   auto& t=out.back();t["renderer"]="LiveV1 / prepared PCM / independent WASAPI";t["recoveryRevision"]="LiveV1";t["detail"]=flags?std::string(d.detail.data()):std::string{};
   t["independentDomains"]=true;t["peerGatesPlayback"]=false;t["faultPolicy"]="Local domain only; all-domain source hold remains explicit";
   t["fifoFrames"]=d.core.input.readable();t["fifoCapacity"]=65536;t["targetFrames"]=d.target.load();t["readyPackets"]=d.core.ready.size();t["readyCapacityPackets"]=4;
   t["totalUnsubmittedSourceFrames"]=d.core.pendingSourceFrames();t["controlLevelFrames"]=d.core.observedLevel();t["clockFillFrames"]=d.core.pendingSourceFrames();t["fillScope"]="All source frames not yet committed to WASAPI; not an internal reservoir";
   t["transportBufferedMs"]=(d.core.pendingSourceFrames()+d.quantum.load())/192.0;t["dataAgeMs"]=0.0;t["dataAgeValid"]=false;
   t["ppm"]=d.core.appliedPpm();t["applied_ppm"]=d.core.appliedPpm();t["requested_ppm"]=d.core.requestedPpm();t["rateControlUpdates"]=J::u64(d.core.rateUpdates());
   t["clockState"]=d.running.load()?1:0;t["clockControllerLocked"]=d.running.load()&&d.core.rateUpdates()>0;t["clockEstimateValid"]=false;t["estimatedSinkRateHz"]=0.0;t["clockReadingValid"]=d.clockValid.load();t["clockFrequency"]=J::u64(d.clockFrequency.load());
   t["xruns"]=J::u64(d.xruns.load());t["wasapiTimeouts"]=J::u64(d.timeouts.load());t["wasapiXruns"]=J::u64(d.timeouts.load());t["wasapiDeviceLosses"]=J::u64(0);t["resyncCount"]=J::u64(0);t["lastResetReason"]=0;
   t["callbackWallExcludingWaitUs"]=d.callbackUs.load();t["maximumCallbackWallExcludingWaitUs"]=d.maxCallbackUs.load();t["serviceWallToSubmitUs"]=d.callbackUs.load();t["maximumServiceWallToSubmitUs"]=d.maxCallbackUs.load();t["eventWaitUs"]=d.waitUs.load();t["deadlineMisses"]=J::u64(d.misses.load());t["callbackThreadCycles"]=J::u64(0);
   t["preparationWallUs"]=d.prepareUs.load();t["maximumPreparationWallUs"]=d.maxPrepareUs.load();t["callbackWallScope"]="Prepared PCM copy/submit/accounting only; ASRC and PCM24 on separate preparation owner";
   t["softwareAlignmentVerified"]=false;t["analogSkewMeasured"]=false;t["estimatedABDifferenceFrames"]=0.0;t["abDifferenceAvailable"]=false;t["presentationQpc100ns"]=J::u64(0);t["commonPresentationQpc100ns"]=J::u64(0);t["audiblePresentationQpc100ns"]=J::u64(0);t["mappingCoherent"]=false;t["firstGraphFrame"]=d.firstGraph.load();t["endGraphFrame"]=d.endGraph.load();t["epoch"]=J::u64(d.core.input.epoch());
   t["streamLatency100ns"]=J::u64(uint64_t(d.quantum.load())*10000000/192000);t["streamLatencyValid"]=false;t["originalClockCalibrated"]=false;t["nonzeroPcmBlocks"]=J::u64(0);t["nonzeroCounterAvailable"]=false;
   t["pcmDitherMode"]=pcm.dither_mode;t["pcmSeedMode"]=pcm.seed_mode;t["pcmFaults"]=pcm.fault_flags;t["pcmProcessedFrames"]=J::u64(pcm.processed_frames);t["pcmOverrangeSamples"]=pcm.overrange_sample_count;t["pcmRailClampSamples"]=J::u64(pcm.reserved[0]);t["pcmClippedBlocks"]=J::u64(pcm.reserved[2]);t["pcmNonfiniteSamples"]=J::u64(pcm.nonfinite_sample_count);
  }return out;
 }
};
LiveSinkWorkers::LiveSinkWorkers(Factory factory,bool):impl_(std::make_unique<Impl>(std::move(factory))){}
LiveSinkWorkers::~LiveSinkWorkers()=default;
bool LiveSinkWorkers::prepare(const std::array<LiveDomain,2>& c){return impl_->prepare(c);}
bool LiveSinkWorkers::start(){return impl_->start();}
void LiveSinkWorkers::stop() noexcept {impl_->stop();}
void LiveSinkWorkers::closePublicationGate() noexcept {impl_->closeGate();}
void LiveSinkWorkers::atBlockBoundary() noexcept {impl_->boundary();}
void LiveSinkWorkers::graphStopped() noexcept {impl_->attached=nullptr;impl_->graphRunning=false;impl_->ack=impl_->request.load();}
void LiveSinkWorkers::publish(const double* const* p,unsigned n,bool policy) noexcept {egress_host::GraphInterval v{1,impl_->legacyFrame,0,n};impl_->legacyFrame+=n;impl_->publish(p,n,policy,v);}
void LiveSinkWorkers::publish(const double* const* p,unsigned n,bool policy,egress_host::GraphInterval v) noexcept {impl_->publish(p,n,policy,v);}
void LiveSinkWorkers::failAll(unsigned f) noexcept {impl_->globalFail(f,false);}
void LiveSinkWorkers::waitForInput() noexcept {impl_->globalFail(egress_host::Input,true);}
bool LiveSinkWorkers::sourceWaitFault() const noexcept {return impl_->fault.load()&&impl_->session&&impl_->session->sourceHold.load();}
unsigned LiveSinkWorkers::sourceWaitFaults() const noexcept {unsigned flags=0;if(impl_->session)for(const auto& d:impl_->session->domains)if(d.config.enabled)flags|=d.flags.load();return flags;}
bool LiveSinkWorkers::armed() const noexcept {return impl_->enabled.load()&&impl_->runningMask.load()!=0;}
bool LiveSinkWorkers::recoveryAtTarget() const noexcept {return false;} // no legacy clock-resync policy
bool LiveSinkWorkers::faulted() const noexcept {return impl_->fault.load(std::memory_order_acquire);}
bool LiveSinkWorkers::engaged() const noexcept {return impl_->enabled.load(std::memory_order_acquire);}
ipc::Value LiveSinkWorkers::snapshot() const {return impl_->snapshot();}
uint32_t LiveSinkWorkers::renderLaneMask(const std::array<LiveDomain,2>& domains) noexcept {uint32_t mask=0;for(const auto& d:domains)if(d.enabled)for(unsigned c=0;c<d.channels&&c<16;++c)if(d.lanes[c]<16)mask|=1u<<d.lanes[c];return mask;}
std::array<LiveDomain,2> LiveSinkWorkers::decode(const ipc::Value& value){
 std::array<LiveDomain,2> result{};const auto& domains=value.asArray();if(domains.size()>2)throw std::invalid_argument("At most two DAC domains");
 for(unsigned i=0;i<domains.size();++i){auto& d=result[i];const auto& v=domains[i];d.enabled=v.at("enabled").asBool();if(!d.enabled)continue;
  d.endpoint=v.at("endpoint").asString();d.logicalId=v.at("logicalId").asString();d.channels=v.at("channels").asUnsigned(16);d.format=v.at("format").asUnsigned(1);
  if(v.at("rate").asUnsigned()!=192000||v.at("lanes").asArray().size()!=d.channels)throw std::invalid_argument("Exact FP64/192 lane contract required");
  for(unsigned c=0;c<d.channels;++c)d.lanes[c]=v.at("lanes").asArray()[c].asUnsigned(15);
  if(v.contains("liveAllowed"))d.liveAllowed=v.at("liveAllowed").asBool();if(v.contains("liveError"))d.liveError=v.at("liveError").asString();
 }return result;
}
}
