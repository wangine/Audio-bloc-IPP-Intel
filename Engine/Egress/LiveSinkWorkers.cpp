#include "LiveSinkWorkers.h"
#include <entropy/dac/WasapiDacSink.h>
#include <windows.h>
#include <avrt.h>
#include <algorithm>
#include <cstring>
namespace entropy::control {
namespace {
class WasapiAdapter final:public egress_host::Sink {
 entropy::dac::WasapiDacSink sink_;
public:
 EntropyResult open(const EntropyDacSinkConfig& c) override{return sink_.open(c);}
 EntropyResult start() noexcept override{return sink_.start();}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{return sink_.waitBuffer(r);}
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{return sink_.submit(b);}
 EntropyDacSinkStatus status() const noexcept override{return sink_.status();}
 void stop() noexcept override{sink_.stop();}
 void close() noexcept override{sink_.close();}
};
class MeasuredSink final:public egress_host::Sink {
 egress_host::Sink& sink_;double frequency_;
public:
 double waitUs=0;uint64_t nonzeroPcmBlocks=0;
 explicit MeasuredSink(egress_host::Sink& s):sink_(s){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);frequency_=double(f.QuadPart);}
 EntropyResult open(const EntropyDacSinkConfig& c) override{return sink_.open(c);}
 EntropyResult start() noexcept override{return sink_.start();}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);auto result=sink_.wait(r);QueryPerformanceCounter(&b);waitUs=double(b.QuadPart-a.QuadPart)*1e6/frequency_;return result;}
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{const auto result=sink_.submit(b);if(result==ENTROPY_OK&&b.accepted_frames==b.frames){const auto* bytes=static_cast<const uint8_t*>(b.data);if(std::any_of(bytes,bytes+b.byte_count,[](uint8_t v){return v!=0;}))++nonzeroPcmBlocks;}return result;}
 EntropyDacSinkStatus status() const noexcept override{return sink_.status();}
 void stop() noexcept override{sink_.stop();}
 void close() noexcept override{sink_.close();}
};
}
LiveSinkWorkers::LiveSinkWorkers(Factory f):factory_(std::move(f)){}
std::array<LiveDomain,2> LiveSinkWorkers::decode(const ipc::Value& v){
 std::array<LiveDomain,2> result{};auto& list=v.asArray();if(list.size()>2)throw std::invalid_argument("At most DAC A/B");
 for(unsigned i=0;i<list.size();++i){auto& d=result[i];auto& j=list[i];d.enabled=j.at("enabled").asBool();if(!d.enabled)continue;
  d.endpoint=j.at("endpoint").asString();d.logicalId=j.at("logicalId").asString();d.channels=j.at("channels").asUnsigned(16);d.format=j.at("format").asUnsigned(1);
  if(j.at("rate").asUnsigned()!=192000||j.at("lanes").asArray().size()!=d.channels)throw std::invalid_argument("Exact 192 kHz channel mapping required");
  for(unsigned c=0;c<d.channels;++c)d.lanes[c]=j.at("lanes").asArray()[c].asUnsigned(15);
 }return result;
}
bool LiveSinkWorkers::prepare(const std::array<LiveDomain,2>& configs){
 stop();prepared_=false;std::array<bool,16> used{};unsigned count=0;
 for(unsigned i=0;i<2;++i){const auto& c=configs[i];if(!c.enabled)continue;++count;
  if(c.channels<1||c.channels>16||c.format>1||c.endpoint.empty()||c.endpoint.size()>=ENTROPY_DAC_SINK_ENDPOINT_ID_BYTES||c.logicalId!=(i?"DAC_B":"DAC_A"))return false;
  if(i&&configs[0].enabled&&c.endpoint==configs[0].endpoint)return false;
  for(unsigned n=0;n<c.channels;++n){if(c.lanes[n]>15||used[c.lanes[n]])return false;used[c.lanes[n]]=true;}
 }
 if(!count)return false;
 for(unsigned i=0;i<2;++i){auto& d=domains_[i];d.config=configs[i];d.nonzeroPcmBlocks=0;d.callbacks=0;d.misses=0;d.cycles=0;d.mmcss=false;d.open=false;d.running=false;d.buffer=0;d.sinkState=ENTROPY_DAC_SINK_CLOSED;d.sinkFaults=0;d.driverResult=0;d.submitted=0;d.position=0;d.ppm=0;d.clockFill=0;d.clockState=0;d.maxCallbackUs=0;d.waitUs=0;d.callbackUs=0;
  if(!d.config.enabled)continue;egress_host::PipelineConfig p;p.channels=d.config.channels;p.format=d.config.format;p.maximumFrames=4096;p.fifoFrames=32768;p.targetFrames=4096;
  if(!d.pipeline.prepare(p))return false;
 }
 faulted_=false;prepared_=true;return true;
}
bool LiveSinkWorkers::start(){
 if(!prepared_||enabled_)return false;
 for(auto& d:domains_)if(d.config.enabled&&!d.pipeline.arm(true,true))return false;
 stopping_=false;enabled_=true;prepared_=false;
 try{for(unsigned i=0;i<2;++i)if(domains_[i].config.enabled)domains_[i].owner=std::thread([this,i]{run(i);});return true;}
 catch(...){stop();return false;}
}
void LiveSinkWorkers::stop() noexcept {
 enabled_=false;stopping_=true;for(auto& d:domains_)d.pipeline.disarm();
 for(auto& d:domains_)if(d.owner.joinable())d.owner.join();
}
void LiveSinkWorkers::failAll(unsigned reason) noexcept {
 faulted_=true;for(auto& d:domains_)if(d.config.enabled)d.pipeline.fail(reason);
}
bool LiveSinkWorkers::armed() const noexcept {
 if(!enabled_.load(std::memory_order_acquire)||faulted_.load(std::memory_order_acquire)||stopping_)return false;
 for(const auto& d:domains_)if(d.config.enabled&&!d.running.load(std::memory_order_acquire))return false;return true;
}
void LiveSinkWorkers::publish(const double* const* output,unsigned frames,bool policy) noexcept {
 if(!enabled_.load(std::memory_order_acquire)||faulted_||stopping_)return;
 if(!policy||!output){failAll(egress_host::Input);return;}
 for(auto& d:domains_)if(d.config.enabled){std::array<const double*,16> selected{};for(unsigned n=0;n<d.config.channels;++n)selected[n]=output[d.config.lanes[n]];
  if(!d.pipeline.publish(selected.data(),frames)){if(!stopping_)failAll(d.pipeline.faults()?d.pipeline.faults():egress_host::Input);return;}}
}
void LiveSinkWorkers::run(unsigned slot) noexcept {
 auto& d=domains_[slot];DWORD task=0;HANDLE mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
 d.mmcss=mmcss&&AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL);
 try{
  if(!d.mmcss){failAll(egress_host::Device);throw std::runtime_error("MMCSS Pro Audio unavailable");}
  auto sink=factory_?factory_(slot):std::make_unique<WasapiAdapter>();if(!sink)throw std::runtime_error("Sink factory unavailable");
  MeasuredSink measured(*sink);egress_host::SinkOwner owner(d.pipeline,measured);
  EntropyDacSinkConfig config{};config.struct_size=sizeof(config);config.backend=ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;config.sample_rate=192000;config.channels=d.config.channels;config.output_format=d.config.format;config.requested_buffer_frames=1024;config.revision=1;
  std::memcpy(config.endpoint_id_utf8,d.config.endpoint.c_str(),d.config.endpoint.size()+1);
  if(!owner.open(config)){auto s=sink->status();d.driverResult=s.last_driver_result;d.sinkFaults=s.fault_flags;failAll(egress_host::Device);}
  else{
   d.open=true;auto status=sink->status();d.buffer=status.buffer_frames;d.sinkState=status.state;
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
   const unsigned preroll=4096+status.buffer_frames+100;
   while(!stopping_&&!faulted_&&d.pipeline.readable()<preroll&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
   if(!stopping_&&!faulted_){if(d.pipeline.readable()<preroll)failAll(egress_host::Starvation);else if(!owner.start())failAll(egress_host::Device);else d.running=true;}
   LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);double maximum=0;
   while(!stopping_&&!faulted_&&d.running){LARGE_INTEGER begin{},end{};ULONG64 ca=0,cb=0;QueryPerformanceCounter(&begin);QueryThreadCycleTime(GetCurrentThread(),&ca);
    const bool ok=owner.cycle();QueryThreadCycleTime(GetCurrentThread(),&cb);QueryPerformanceCounter(&end);
    const double cost=std::max(0.0,double(end.QuadPart-begin.QuadPart)*1e6/double(frequency.QuadPart)-measured.waitUs);maximum=std::max(maximum,cost);
    d.callbackUs=cost;d.maxCallbackUs=maximum;d.waitUs=measured.waitUs;d.cycles=cb-ca;d.nonzeroPcmBlocks=measured.nonzeroPcmBlocks;d.callbacks.fetch_add(1);if(cost>double(d.buffer.load())/192000*1e6)d.misses.fetch_add(1);
    auto clock=d.pipeline.clockTelemetry();d.ppm=clock.correction_ppm;d.clockFill=clock.fill_frames;d.clockState=clock.state;status=sink->status();d.submitted=status.submitted_frames;d.position=status.device_position_frames;d.sinkState=status.state;d.driverResult=status.last_driver_result;d.sinkFaults=status.fault_flags;
    if(!ok&&!stopping_)failAll(d.pipeline.faults()?d.pipeline.faults():egress_host::Device);
   }
   // A group fault must stop the remaining member too, on its own COM owner.
   d.running=false;owner.stop();d.open=false;status=sink->status();d.sinkState=status.state;d.driverResult=status.last_driver_result;
  }
 }catch(...){if(!stopping_)failAll(egress_host::Device);d.running=false;d.open=false;}
 if(mmcss)AvRevertMmThreadCharacteristics(mmcss);
}
ipc::Value LiveSinkWorkers::snapshot() const {
 ipc::Value::Array out;
 for(unsigned i=0;i<2;++i){const auto& d=domains_[i];const bool enabled=d.config.enabled;
  const auto pcm=d.pipeline.pcmTelemetry();
  out.emplace_back(ipc::Value::Object{{"id",i?"DAC B":"DAC A"},{"configured",enabled},{"channels",d.config.channels},{"rate",192000},{"format",d.config.format},{"endpoint",d.config.endpoint},{"offlineFixture",false},{"physicalSinkOpen",d.open.load()},{"state",!enabled||!enabled_.load()?"DISARMED":faulted_.load()?"FAULT":d.running.load()?"RUNNING":d.open.load()?"PREROLL":"ARMING"},{"sinkState",d.sinkState.load()},{"driverResult",d.driverResult.load()},{"sinkFaults",d.sinkFaults.load()},{"bufferFrames",d.buffer.load()},{"submittedFrames",ipc::Value::u64(d.submitted.load())},{"devicePositionFrames",ipc::Value::u64(d.position.load())},{"clockState",d.clockState.load()},{"clockControllerLocked",d.clockState.load()==ENTROPY_EGRESS_CLOCK_LOCKED},{"ppm",d.ppm.load()},{"clockFillFrames",d.clockFill.load()},{"fifoFrames",d.pipeline.readable()},{"fifoCapacity",32768},{"targetFrames",4096},{"xruns",ipc::Value::u64(d.pipeline.xruns())},{"faults",d.pipeline.faults()},{"nonzeroPcmBlocks",ipc::Value::u64(d.nonzeroPcmBlocks.load())},{"callbacks",ipc::Value::u64(d.callbacks.load())},{"callbackWallExcludingWaitUs",d.callbackUs.load()},{"maximumCallbackWallExcludingWaitUs",d.maxCallbackUs.load()},{"eventWaitUs",d.waitUs.load()},{"deadlineMisses",ipc::Value::u64(d.misses.load())},{"callbackThreadCycles",ipc::Value::u64(d.cycles.load())},{"mmcssRegistered",d.mmcss.load()},{"guardian","EXACT BYPASS"}});
  auto& telemetry=out.back();
  telemetry["pcmDitherMode"]=pcm.dither_mode;telemetry["pcmSeedMode"]=pcm.seed_mode;telemetry["pcmFaults"]=pcm.fault_flags;
  telemetry["pcmProcessedFrames"]=ipc::Value::u64(pcm.processed_frames);
  telemetry["pcmOverrangeSamples"]=ipc::Value::u64(pcm.overrange_sample_count);
  telemetry["pcmRailClampSamples"]=ipc::Value::u64(pcm.reserved[0]);
  telemetry["pcmClippedBlocks"]=ipc::Value::u64(pcm.reserved[2]);
  telemetry["pcmNonfiniteSamples"]=ipc::Value::u64(pcm.nonfinite_sample_count);
 }return out;
}
}
