#include "LiveSinkWorkers.h"
#include "Engine/Diagnostics/ChainAudit.h"
#include "PresentationContract.h"
#include <entropy/dac/WasapiDacSink.h>
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cstring>
#ifndef ENTROPY_LIVE_FIFO_FRAMES
#define ENTROPY_LIVE_FIFO_FRAMES 32768
#endif
namespace entropy::control {
namespace {
uint64_t host100ns() noexcept {LARGE_INTEGER q{},f{};QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return uint64_t(static_cast<long double>(q.QuadPart)*10000000/f.QuadPart);}
class WasapiAdapter final:public egress_host::Sink {
 entropy::dac::WasapiDacSink sink_;
public:
 void setCancelEvent(void* e) noexcept override {sink_.setCancelEvent(e);}
 egress_host::SinkTiming timing() const noexcept override {return {sink_.clockFrequency(),sink_.streamLatency100ns(),sink_.freshClock(),sink_.precisePositionFrames(),sink_.latencyValid()};}
 EntropyResult open(const EntropyDacSinkConfig& c) override{return sink_.open(c);}
 EntropyResult start() noexcept override{return sink_.start();}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{return sink_.waitBuffer(r);}
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{return sink_.submit(b);}
 EntropyDacSinkStatus status() const noexcept override{return sink_.status();}
 void stop() noexcept override{sink_.stop();}
 void close() noexcept override{sink_.close();}
};
}
LiveSinkWorkers::LiveSinkWorkers(Factory f,bool calibrate):factory_(std::move(f)),calibrateClock_(calibrate) {
 if(!factory_&&!calibrateClock_)throw std::invalid_argument("Physical sinks require original clock calibration");
 for(auto& e:cancel_){e=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!e){for(auto h:cancel_)if(h)CloseHandle(HANDLE(h));throw std::runtime_error("DAC cancellation event unavailable");}}
}
LiveSinkWorkers::~LiveSinkWorkers(){stop();for(auto e:cancel_)CloseHandle(HANDLE(e));}
uint32_t LiveSinkWorkers::renderLaneMask(const std::array<LiveDomain,2>& domains) noexcept {
 uint32_t mask=0;for(const auto& domain:domains)if(domain.enabled)
  for(unsigned channel=0;channel<domain.channels&&channel<16;++channel)if(domain.lanes[channel]<16)mask|=1u<<domain.lanes[channel];
 return mask;
}
std::array<LiveDomain,2> LiveSinkWorkers::decode(const ipc::Value& v){
 std::array<LiveDomain,2> result{};auto& list=v.asArray();if(list.size()>2)throw std::invalid_argument("At most DAC A/B");
 for(unsigned i=0;i<list.size();++i){auto& d=result[i];auto& j=list[i];d.enabled=j.at("enabled").asBool();if(!d.enabled)continue;
  d.endpoint=j.at("endpoint").asString();d.logicalId=j.at("logicalId").asString();d.channels=j.at("channels").asUnsigned(16);d.format=j.at("format").asUnsigned(1);
  if(j.at("rate").asUnsigned()!=192000||j.at("lanes").asArray().size()!=d.channels)throw std::invalid_argument("Exact 192 kHz channel mapping required");
  for(unsigned c=0;c<d.channels;++c)d.lanes[c]=j.at("lanes").asArray()[c].asUnsigned(15);
 }return result;
}
bool LiveSinkWorkers::detached() const noexcept {return !graphRunning_.load(std::memory_order_acquire)||ack_.load(std::memory_order_acquire)==request_.load(std::memory_order_acquire);}
void LiveSinkWorkers::atBlockBoundary() noexcept {
 graphRunning_.store(true,std::memory_order_release);
 const auto serial=request_.load(std::memory_order_acquire);
 if(serial!=ack_.load(std::memory_order_relaxed)){attached_=requested_.load(std::memory_order_acquire);ack_.store(serial,std::memory_order_release);}
}
void LiveSinkWorkers::graphStopped() noexcept {attached_=nullptr;graphRunning_=false;ack_=request_.load();}
bool LiveSinkWorkers::prepare(const std::array<LiveDomain,2>& configs){
 stop();prepared_=false;if(!detached())return false;
 std::array<bool,16> used{};unsigned count=0;
 for(unsigned i=0;i<2;++i){const auto& c=configs[i];if(!c.enabled)continue;++count;
  if(c.channels<1||c.channels>16||c.format>1||c.endpoint.empty()||c.endpoint.size()>=ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES||c.logicalId!=(i?"DAC_B":"DAC_A"))return false;
  if(i&&configs[0].enabled&&c.endpoint==configs[0].endpoint)return false;
  for(unsigned n=0;n<c.channels;++n){if(c.lanes[n]>15||used[c.lanes[n]])return false;used[c.lanes[n]]=true;}
 }
 if(!count)return false;
 auto candidate=std::make_unique<Session>();
 for(unsigned i=0;i<2;++i){auto& d=candidate->domains[i];d.config=configs[i];if(!d.config.enabled)continue;
  egress_host::PipelineConfig p;p.channels=d.config.channels;p.format=d.config.format;p.maximumFrames=4096;p.fifoFrames=ENTROPY_LIVE_FIFO_FRAMES;p.targetFrames=4096;
  if(!d.pipeline.prepare(p)||(calibrateClock_&&!d.pipeline.prepareSilentCalibration()))return false;
 }
 session_=std::move(candidate);faulted_=false;prepared_=true;return true;
}
bool LiveSinkWorkers::start(){
 if(!prepared_||enabled_||!session_)return false;
 auto& s=*session_;
 for(unsigned i=0;i<2;++i){ResetEvent(HANDLE(cancel_[i]));if(s.domains[i].config.enabled&&!s.domains[i].pipeline.arm(true,true))return false;}
 s.stop=false;enabled_=true;prepared_=false;legacyFrame_=0;
 requested_.store(&s,std::memory_order_release);request_.fetch_add(1,std::memory_order_release);
 try{for(unsigned i=0;i<2;++i)if(s.domains[i].config.enabled){s.domains[i].done=false;s.domains[i].owner=std::thread([this,&s,i]{run(s,i);});}return true;}
 catch(...){stop();return false;}
}
void LiveSinkWorkers::closePublicationGate() noexcept {
 enabled_.store(false,std::memory_order_release);
 for(auto e:cancel_)SetEvent(HANDLE(e));
}
void LiveSinkWorkers::stop() noexcept {
 closePublicationGate();requested_.store(nullptr,std::memory_order_release);request_.fetch_add(1,std::memory_order_release);
 if(session_){session_->stop=true;for(auto& d:session_->domains)if(d.owner.joinable())d.owner.join();}
 // Graph acknowledges retirement at its next boundary, also during input starvation.
 // A timeout retains the resource; prepare refuses replacement until acknowledged.
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!detached()&&std::chrono::steady_clock::now()<limit)std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void LiveSinkWorkers::fail(Session& s,unsigned reason,bool source) noexcept {
 if(source||reason==egress_host::Starvation)s.sourceHold=true;
 else s.fatal=true; // Monotonic provenance: Graph/module Input is also fatal.
 s.fault=true;faulted_=true;for(auto& d:s.domains)if(d.config.enabled)d.pipeline.fail(reason);
 for(auto e:cancel_)SetEvent(HANDLE(e));
}
void LiveSinkWorkers::failAll(unsigned reason) noexcept {if(attached_)fail(*attached_,reason);}
void LiveSinkWorkers::waitForInput() noexcept {if(attached_)fail(*attached_,egress_host::Input,true);}
unsigned LiveSinkWorkers::sourceWaitFaults() const noexcept {
 unsigned flags=0;if(session_)for(const auto& d:session_->domains)if(d.config.enabled)flags|=d.pipeline.faults();return flags;
}
bool LiveSinkWorkers::sourceWaitFault() const noexcept {
 const auto flags=sourceWaitFaults();return faulted_&&session_&&!session_->fatal&&session_->sourceHold&&flags&&!(flags&~unsigned(egress_host::Input|egress_host::Starvation));
}
bool LiveSinkWorkers::armed() const noexcept {
 if(!enabled_||faulted_||!session_||session_->stop)return false;
 for(const auto& d:session_->domains)if(d.config.enabled&&!d.running)return false;return true;
}
bool LiveSinkWorkers::recoveryAtTarget() const noexcept {if(!armed())return false;for(const auto& d:session_->domains)if(d.config.enabled&&d.clockFill>=4096)return true;return false;}
void LiveSinkWorkers::publish(const double* const* outputs,unsigned frames,bool policy) noexcept {
 // Offline injected producers have the same boundary contract, without a scheduler.
 if(!graphRunning_){attached_=requested_.load();ack_=request_.load();}
 egress_host::GraphInterval v{1,legacyFrame_,host100ns(),frames};legacyFrame_+=frames;publish(outputs,frames,policy,v);
}
void LiveSinkWorkers::publish(const double* const* output,unsigned frames,bool policy,egress_host::GraphInterval interval) noexcept {
 auto* s=attached_;if(!s||!enabled_.load(std::memory_order_acquire)||s->stop||s->fault)return;
 if(!policy||!output){fail(*s,egress_host::Input);return;}
 for(const auto& d:s->domains)if(d.config.enabled&&!d.calibrated.load(std::memory_order_acquire))return; // inactive preroll, no queued input discarded
 if(s->publishedBlocks.load(std::memory_order_relaxed)==0){s->baseFrame=interval.firstGraphFrame;s->baseEpoch=interval.epoch;s->baseCapture=interval.capture100ns;}
 if(interval.epoch!=s->baseEpoch.load(std::memory_order_relaxed)){fail(*s,egress_host::Clock);return;}
 for(auto& d:s->domains)if(d.config.enabled){std::array<const double*,16> selected{};for(unsigned n=0;n<d.config.channels;++n)selected[n]=output[d.config.lanes[n]];
  if(!d.pipeline.publish(selected.data(),frames,interval)){if(enabled_)fail(*s,d.pipeline.faults()?d.pipeline.faults():egress_host::Input);return;}}
 s->lastGraphEnd=interval.firstGraphFrame+frames;s->publishedBlocks.fetch_add(1,std::memory_order_release);
}
void LiveSinkWorkers::run(Session& s,unsigned slot) noexcept {
 auto& d=s.domains[slot];DWORD task=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
 d.mmcss=mmcss&&AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 const auto canceled=[&]{return s.stop.load()||s.fault.load()||WaitForSingleObject(HANDLE(cancel_[slot]),0)==WAIT_OBJECT_0;};
 try{
  if(!d.mmcss)throw std::runtime_error("MMCSS Pro Audio unavailable");
  auto sink=factory_?factory_(slot):std::make_unique<WasapiAdapter>();if(!sink)throw std::runtime_error("Sink unavailable");
  sink->setCancelEvent(cancel_[slot]);
  EntropyDacSinkConfig c{};c.struct_size=sizeof(c);c.backend=ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;c.sample_rate=192000;c.channels=d.config.channels;c.output_format=d.config.format;c.requested_buffer_frames=1024;c.revision=1;std::memcpy(c.endpoint_id_utf8,d.config.endpoint.c_str(),d.config.endpoint.size()+1);
  struct CloseOnOwner {egress_host::Sink& sink;Domain& d;~CloseOnOwner(){const auto v=sink.status();d.driverResult=v.last_driver_result;d.sinkFaults=v.fault_flags;sink.stop();sink.close();}} closer{*sink,d};
  if(sink->open(c)!=ENTROPY_OK)throw std::runtime_error("Sink open failed");
  auto status=sink->status();if(!status.buffer_frames||status.buffer_frames>4096)throw std::runtime_error("Sink buffer invalid");
  // The original sinc requires a lookahead guard. If one callback exceeds the
  // fixed fill target, the unmodified servo would continuously trim this extra
  // backlog. Reject that unsupported profile before Start, without changing its
  // controller law or hiding the failed 4096-buffer diagnostic.
  if(status.buffer_frames+100>d.pipeline.config().targetFrames)throw std::runtime_error("Sink buffer plus sinc guard exceeds 4096 target; group ARM refused");
  d.buffer=status.buffer_frames;d.sinkState=status.state;d.open.store(true,std::memory_order_release);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
  unsigned coordinator=0;while(coordinator<2&&!s.domains[coordinator].config.enabled)++coordinator;
  // Both endpoints must be OPEN before either Start; initial driver buffer is SILENT.
  while(!canceled()){
   bool ready=true;for(const auto& other:s.domains)if(other.config.enabled&&!other.open.load(std::memory_order_acquire))ready=false;
   if(ready)break;if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Group open timeout");WaitForSingleObject(HANDLE(cancel_[slot]),1);
  }
  if(!canceled()&&sink->start()!=ENTROPY_OK)throw std::runtime_error("Sink start failed");
  uint64_t nextDeviceFrame=d.buffer.load(); // account for initial SILENT buffer
  bool primed=false;double maximum=0;
  while(!canceled()){
   EntropyDacSinkBufferRequest req{};req.struct_size=sizeof(req);req.timeout_ms=100;
   const auto waitBegin=host100ns();const auto waitResult=sink->wait(req);d.waitUs=double(host100ns()-waitBegin)/10;
   if(canceled())break;if(waitResult!=ENTROPY_OK)throw std::runtime_error("Sink wait failed");
   if(req.frames!=d.buffer||req.bytes!=d.pipeline.bytes(req.frames).size())throw std::runtime_error("Sink request invalid");
   const auto timing=sink->timing();
   const bool valid=timing.freshClock&&timing.clockFrequency&&req.device_qpc_position&&nextDeviceFrame>=req.device_position_frames;
   d.clockValid=valid;d.latencyValid=timing.latencyValid;d.clockFrequency=timing.clockFrequency;d.latency100ns=timing.latency100ns;
   if(!valid)throw std::runtime_error("No fresh hardware clock; presentation unverified");
   if(!primed&&!d.pipeline.haveGraphStream()){const bool calibrated=!calibrateClock_||d.pipeline.observeSilentClock(req.device_position_frames,req.device_qpc_position,req.frames);if(calibrated)d.calibrated=true;}
   if(d.pipeline.estimateValid()&&d.pipeline.estimatedSinkRate()>0)d.rateHz=d.pipeline.estimatedSinkRate();
   const double rate=d.rateHz.load();
   const auto presentation=egress_host::PresentationContract::bufferTime(req.device_qpc_position,timing.precisePositionFrames>=0?timing.precisePositionFrames:double(req.device_position_frames),nextDeviceFrame,0,rate);
   d.clockReady=presentation;
   if(slot==coordinator&&!s.presentation.load(std::memory_order_acquire)){
    bool ready=s.publishedBlocks.load(std::memory_order_acquire)!=0;uint64_t latest=0;unsigned largest=0;
    for(const auto& other:s.domains)if(other.config.enabled){largest=(std::max)(largest,other.buffer.load());latest=(std::max)(latest,other.clockReady.load());if(!other.clockReady||other.pipeline.readable()<4096+3*other.buffer.load()+2048)ready=false;}
    // Keep one buffer for callback phase and one for the producer/peer boundary.
    // This reserve is external to the unchanged 4096-frame servo target. A late
    // owner may replan only while silent; it must never prime beyond known data.
    // Advancing the hardware presentation time must not advance the source
    // position into PCM that has not arrived. The previous +leadFrames term
    // required future source packets to prime, then accumulated unread data
    // through repeated silent replans until one DAC FIFO overflowed.
    if(ready){const uint64_t leadFrames=uint64_t(largest)*3+192;const auto lead=leadFrames*10000000/192000;s.referenceFrame=s.lastGraphEnd.load()-4096-2*largest;s.presentation.store(latest+lead,std::memory_order_release);}
   }
   if(slot==coordinator&&s.primedMask.load(std::memory_order_acquire)&&!s.audiblePresentation.load()){
    unsigned required=0,largest=0;uint64_t latest=0;for(unsigned i=0;i<2;++i)if(s.domains[i].config.enabled){required|=1u<<i;largest=(std::max)(largest,s.domains[i].buffer.load());latest=(std::max)(latest,s.domains[i].clockReady.load());}
    if(s.primedMask.load()==required)s.audiblePresentation=latest+uint64_t(largest)*3*10000000/192000+10000;
   }
   const auto common=s.presentation.load(std::memory_order_acquire);
   const auto begin=host100ns();ULONG64 ca=0,cb=0;QueryThreadCycleTime(GetCurrentThread(),&ca);
   bool audio=false;unsigned offset=0;
   if(common&&presentation+uint64_t(double(req.frames)*10000000/rate)>common){
    if(presentation<common)offset=unsigned(std::ceil(double(common-presentation)*rate/10000000));
    if(offset<req.frames){
     const auto firstTime=presentation+uint64_t(double(offset)*10000000/rate);
     const double first=egress_host::PresentationContract::graphPosition(s.referenceFrame,common,firstTime);
     if(!primed){
      // A single nonblocking CAS serializes startup decisions. Busy owners submit
      // silence; no spin, mutex or wait is introduced in the callback.
      unsigned vacant=0;
      if(s.startupDecision.compare_exchange_strong(vacant,slot+1)){
       if(common==s.presentation.load(std::memory_order_acquire)){
        d.primeWanted=first;const bool ready=d.pipeline.primeGraphPosition(first,req.frames-offset);d.primeBase=double(d.pipeline.streamInterval().firstGraphFrame);d.primeNominal=double(d.pipeline.clockTelemetry().nominal_frames);
        if(!ready){
         if(!s.primedMask.load()){s.presentation=0;++s.replans;}
         else fail(s,egress_host::Clock); // already audible: exact local fault + explicit re-ARM
        }else {d.startupSkipped=uint64_t(first-double(s.baseFrame.load()));primed=true;s.primedMask.fetch_or(1u<<slot);}
       }
       s.startupDecision.store(0,std::memory_order_release);
      }
     }
     if(primed){
      d.pipeline.feedUntilTarget(req.frames-offset);
      if(!d.pipeline.renderOffset(req.frames,offset,req.device_position_frames,req.device_qpc_position))throw std::runtime_error("Local egress discontinuity; explicit re-preroll required");
      d.mappingVersion.fetch_add(1,std::memory_order_acq_rel);d.firstFrame=d.pipeline.lastRenderFirstGraphFrame();d.endFrame=d.pipeline.graphReadPosition();d.presentation100ns=firstTime;d.epoch=d.pipeline.streamInterval().epoch;d.capture100ns=d.pipeline.streamInterval().capture100ns;d.mappingVersion.fetch_add(1,std::memory_order_release);
      audio=true;
     }
    }
   }
   const auto onset=s.audiblePresentation.load(std::memory_order_acquire);
   if(audio){
    const auto endTime=presentation+uint64_t(double(req.frames)*10000000/rate);
    if(!onset||endTime<=onset){d.pipeline.silentBuffer(req.frames);audio=false;}
    else if(presentation<onset){const auto prefix=(std::min)(req.frames,unsigned(std::ceil(double(onset-presentation)*rate/10000000)));d.pipeline.silencePrefix(req.frames,prefix);d.running=true;}
    else d.running=true;
   }
   if(!audio)d.pipeline.silentBuffer(req.frames);
   if(canceled()){d.pipeline.silentBuffer(req.frames);audio=false;}
   auto bytes=d.pipeline.bytes(req.frames);EntropyDacSinkPcmBlock block{};block.struct_size=sizeof(block);block.frames=req.frames;block.data=bytes.data();block.byte_count=unsigned(bytes.size());
   auto* audit=req.frames<=1024?chain_audit::sink(slot):nullptr;
   if(audit&&req.frames<=1024){
    audit->qpc=chain_audit::tick();audit->deviceFrame=req.device_position_frames;audit->host100ns=req.device_qpc_position;audit->begin100ns=begin;
    audit->firstGraph=d.pipeline.lastRenderFirstGraphFrame();audit->endGraph=d.pipeline.graphReadPosition();audit->frames=req.frames;audit->channels=d.config.channels;audit->format=d.config.format;audit->audio=audio;audit->offset=offset;
    audit->fifo=d.pipeline.readable();audit->faults=d.pipeline.faults();audit->resetReason=d.pipeline.resetReason();const auto clk=d.pipeline.clockTelemetry();audit->resync=clk.resync_count;audit->ppm=clk.correction_ppm;audit->clockFill=clk.fill_frames;
    for(unsigned ch=0;ch<d.config.channels;++ch)std::memcpy(audit->rendered[ch],d.pipeline.rendered().readChannel(ch),req.frames*sizeof(double));std::memcpy(audit->pcm,bytes.data(),bytes.size());
   }
   const auto submitResult=sink->submit(block);
   if(audit){audit->accepted=block.accepted_frames;audit->submitResult=submitResult;audit->end100ns=host100ns();chain_audit::sinkDone(slot);}
   if(submitResult!=ENTROPY_OK||block.accepted_frames!=req.frames)throw std::runtime_error("Sink submit failed");
   if(audio&&std::any_of(bytes.begin(),bytes.end(),[](uint8_t b){return b!=0;}))++d.nonzeroPcmBlocks;
   nextDeviceFrame+=req.frames;QueryThreadCycleTime(GetCurrentThread(),&cb);const double cost=double(host100ns()-begin)/10;maximum=(std::max)(maximum,cost);
   d.callbackUs=cost;d.maxCallbackUs=maximum;d.cycles=cb-ca;++d.callbacks;if(cost>double(req.frames)*1e6/192000)++d.misses;
   const auto clock=d.pipeline.clockTelemetry();d.ppm=clock.correction_ppm;d.clockFill=clock.fill_frames;d.clockState=clock.state;d.resync=clock.resync_count;d.resetReason=d.pipeline.resetReason();d.estimateValid=d.pipeline.estimateValid();
   status=sink->status();d.submitted=status.submitted_frames;d.position=status.device_position_frames;d.sinkState=status.state;d.driverResult=status.last_driver_result;d.sinkFaults=status.fault_flags;
   if(!common&&std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Group preroll timeout");
  }
 }catch(...){if(!canceled())fail(s,d.pipeline.faults()?d.pipeline.faults():egress_host::Device);}
 d.resync=d.pipeline.clockTelemetry().resync_count;d.resetReason=d.pipeline.resetReason();d.pipeline.disarm();d.running=false;d.open=false;d.sinkState=ENTROPY_DAC_SINK_CLOSED;d.done.store(true,std::memory_order_release);
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
ipc::Value LiveSinkWorkers::snapshot() const {
 ipc::Value::Array out;if(!session_)return out;const auto& s=*session_;const auto now=host100ns();
 struct Mapping {double first=0;uint64_t time=0,epoch=0,capture=0;bool coherent=false;};
 const auto readMapping=[](const Domain& d){Mapping m;for(unsigned k=0;k<3;++k){const auto serial=d.mappingVersion.load(std::memory_order_acquire);if(serial&1)continue;m.first=d.firstFrame.load();m.time=d.presentation100ns.load();m.epoch=d.epoch.load();m.capture=d.capture100ns.load();if(serial==d.mappingVersion.load(std::memory_order_acquire)){m.coherent=true;break;}}return m;};
 const std::array<Mapping,2> mapping{readMapping(s.domains[0]),readMapping(s.domains[1])};
 const auto& a=s.domains[0];const auto& b=s.domains[1];const auto ta=mapping[0].time,tb=mapping[1].time;
 const double difference=(ta&&tb&&mapping[0].coherent&&mapping[1].coherent)?(mapping[0].first-mapping[1].first)-double(int64_t(ta)-int64_t(tb))*192000/10000000:0;
 const bool verified=mapping[0].coherent&&mapping[1].coherent&&a.running&&b.running&&a.clockValid&&b.clockValid&&a.estimateValid&&b.estimateValid&&a.latencyValid&&b.latencyValid&&ta&&tb&&now>ta&&now>tb&&now-ta<1000000&&now-tb<1000000&&std::abs(difference)<=1;
 for(unsigned i=0;i<2;++i){const auto& d=s.domains[i];const auto pcm=d.pipeline.pcmTelemetry();
  out.emplace_back(ipc::Value::Object{{"id",i?"DAC B":"DAC A"},{"configured",d.config.enabled},{"channels",d.config.channels},{"rate",192000},{"format",d.config.format},{"endpoint",d.config.endpoint},{"offlineFixture",false},{"physicalSinkOpen",d.open.load()},{"state",!d.done.load()&&!enabled_.load()?"STOPPING":s.fault.load()?"FAULT":d.running.load()?"RUNNING":d.open.load()?"PREROLL":enabled_.load()?"ARMING":"DISARMED"},{"sinkState",d.sinkState.load()},{"driverResult",d.driverResult.load()},{"sinkFaults",d.sinkFaults.load()},{"bufferFrames",d.buffer.load()},{"submittedFrames",ipc::Value::u64(d.submitted.load())},{"devicePositionFrames",ipc::Value::u64(d.position.load())},{"clockState",d.clockState.load()},{"clockControllerLocked",d.clockState.load()==ENTROPY_EGRESS_CLOCK_LOCKED},{"ppm",d.ppm.load()},{"clockFillFrames",d.clockFill.load()},{"fifoFrames",d.pipeline.readable()},{"fifoCapacity",d.pipeline.config().fifoFrames},{"targetFrames",4096},{"xruns",ipc::Value::u64(d.pipeline.xruns())},{"faults",d.pipeline.faults()},{"nonzeroPcmBlocks",ipc::Value::u64(d.nonzeroPcmBlocks.load())},{"callbacks",ipc::Value::u64(d.callbacks.load())},{"callbackWallExcludingWaitUs",d.callbackUs.load()},{"maximumCallbackWallExcludingWaitUs",d.maxCallbackUs.load()},{"eventWaitUs",d.waitUs.load()},{"deadlineMisses",ipc::Value::u64(d.misses.load())},{"callbackThreadCycles",ipc::Value::u64(d.cycles.load())},{"mmcssRegistered",d.mmcss.load()},{"guardian","EXACT BYPASS"}});
  auto& t=out.back();t["resyncCount"]=ipc::Value::u64(d.resync.load());t["mappingCoherent"]=mapping[i].coherent;t["clockEstimateValid"]=d.estimateValid.load();t["lastResetReason"]=d.resetReason.load();t["epoch"]=ipc::Value::u64(mapping[i].epoch);t["firstGraphFrame"]=mapping[i].first;t["endGraphFrame"]=d.endFrame.load();t["presentationQpc100ns"]=ipc::Value::u64(mapping[i].time);t["commonPresentationQpc100ns"]=ipc::Value::u64(s.presentation.load());t["audiblePresentationQpc100ns"]=ipc::Value::u64(s.audiblePresentation.load());t["clockFrequency"]=ipc::Value::u64(d.clockFrequency.load());t["streamLatency100ns"]=ipc::Value::u64(d.latency100ns.load());t["streamLatencyValid"]=d.latencyValid.load();t["dataAgeMs"]=mapping[i].capture&&now>=mapping[i].capture?double(now-mapping[i].capture)/10000:0;t["clockReadingValid"]=d.clockValid.load();t["originalClockCalibrated"]=d.calibrated.load();t["estimatedSinkRateHz"]=d.rateHz.load();t["primeWantedFrame"]=d.primeWanted.load();t["primeBaseFrame"]=d.primeBase.load();t["primeNominalFrames"]=d.primeNominal.load();t["estimatedABDifferenceFrames"]=difference;t["softwareAlignmentVerified"]=verified;t["analogSkewMeasured"]=false;t["startupSkippedFrames"]=ipc::Value::u64(d.startupSkipped.load());t["groupReplans"]=s.replans.load();
  t["pcmDitherMode"]=pcm.dither_mode;t["pcmSeedMode"]=pcm.seed_mode;t["pcmFaults"]=pcm.fault_flags;t["pcmProcessedFrames"]=ipc::Value::u64(pcm.processed_frames);t["pcmOverrangeSamples"]=ipc::Value::u64(pcm.overrange_sample_count);t["pcmRailClampSamples"]=ipc::Value::u64(pcm.reserved[0]);t["pcmClippedBlocks"]=ipc::Value::u64(pcm.reserved[2]);t["pcmNonfiniteSamples"]=ipc::Value::u64(pcm.nonfinite_sample_count);
 }return out;
}
}
