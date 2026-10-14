#pragma once
#include "EgressContract.h"
#include "Engine/AudioGraph.h"
#include <EgressClockEngine.h>
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <entropy/dac_sink_abi.h>
#include <array>
#include <atomic>
#include <span>
#include <vector>
namespace entropy::egress_host {
struct PipelineConfig {
 unsigned maximumFrames=1024,channels=2,fifoFrames=16384,targetFrames=2048;
 unsigned sinkRate=192000,format=ENTROPY_PCM24_PACKED_LE;bool guardianBypass=true;
};
// Legacy/offline pipeline retained for compatibility; not used by LiveV1.
class DacPipeline final {
public:
 bool prepare(const PipelineConfig&);
 bool arm(bool headroomReady,bool profileAllowed) noexcept;
 void disarm() noexcept;void fail(unsigned reason) noexcept;
 bool publish(const double* const*,unsigned frames) noexcept;
 bool publish(const double* const*,unsigned frames,GraphInterval) noexcept;
 bool prepareSilentCalibration() noexcept;
 bool observeSilentClock(uint64_t p,uint64_t q,unsigned n) noexcept {return clock_.observeSilentClock(p,q,n);}
 bool haveGraphStream() const noexcept {return haveStream_;}
 bool estimateValid() const noexcept {return clock_.clockEstimateOnOwner().valid;}
 unsigned resetReason() const noexcept {return clock_.lastResetReasonOnOwner();}
 double estimatedSinkRate() const noexcept {return clock_.clockEstimateOnOwner().sinkFramesPerSecond;}
 void silencePrefix(unsigned frames,unsigned prefix) noexcept;
 bool renderOffset(unsigned frames,unsigned offset,uint64_t p,uint64_t q) noexcept;
 bool primeGraphPosition(double first,unsigned frames) noexcept;
 double graphReadPosition() const noexcept {return double(stream_.firstGraphFrame)+clock_.nominalReadPosition()-silentPrefix_;}
 GraphInterval streamInterval() const noexcept {auto s=stream_;if(contract_&&transferred_){const auto n=static_cast<uint64_t>((std::max)(0.0,std::floor(lastRenderFirst_-double(stream_.firstGraphFrame))));s.capture100ns=metadataRing_[n&(config_.fifoFrames-1)].capture100ns;}return s;}
 double lastRenderFirstGraphFrame() const noexcept {return lastRenderFirst_;}
 void silentBuffer(unsigned frames) noexcept {silence(frames);}
 bool transfer(unsigned frames) noexcept;unsigned feedUntilTarget(unsigned renderFrames) noexcept;
 bool render(unsigned frames,uint64_t position,uint64_t qpc100ns) noexcept;
 State state() const noexcept {return state_.load(std::memory_order_acquire);}
 unsigned faults() const noexcept {return faults_.load();}uint64_t xruns() const noexcept {return xruns_.load();}
 unsigned writable() const noexcept {return fifo_.writable();}unsigned readable() const noexcept {return fifo_.readable();}
 const PipelineConfig& config() const noexcept {return config_;}
 const EntropyEgressClockConfig& clockConfig() const noexcept {return clock_.config();}
 entropy::egress::ClockEstimator::Quality clockQuality() const noexcept {return clock_.clockQuality();}
 entropy::egress::RatioController::Quality ratioQuality() const noexcept {return clock_.ratioQuality();}
 EntropyEgressClockTelemetry clockTelemetry() const noexcept {return clock_.telemetry();}
 EntropyPcm24Telemetry pcmTelemetry() const noexcept {return pcm_.telemetry();}
 const entropy::engine::PlanarBuffer& rendered() const noexcept {return rendered_;}
 std::span<const uint8_t> bytes(unsigned frames) const noexcept;
 static void guardianBypass(const double* const*,double* const*,unsigned channels,unsigned frames) noexcept;
private:
 bool active() const noexcept;void silence(unsigned frames) noexcept;
 PipelineConfig config_{};SpscAudioFifo fifo_;entropy::egress::EgressClockEngine clock_;entropy::pcm24::Pcm24ModuleEngine pcm_;
 entropy::engine::PlanarBuffer transfer_,rendered_;std::vector<uint8_t> pcmBytes_;
 std::vector<GraphInterval> metadataTransfer_,metadataRing_;
 std::atomic<State> state_{State::Disarmed};std::atomic<unsigned> faults_{None};std::atomic<uint64_t> xruns_{0};
 GraphInterval stream_{};uint64_t transferred_=0,published_=0;unsigned silentPrefix_=0;
 bool haveStream_=false,contract_=false;double lastRenderFirst_=0;uint64_t nominalAtLastRender_=0;
};
class Input11 final {
public:
 void prepare(unsigned frames){zero_.assign(frames,0.0);}
 const double* const* map(const double* const* eleven) noexcept {if(!eleven)return nullptr;for(unsigned c=0;c<11;++c){if(!eleven[c])return nullptr;view_[c]=eleven[c];}view_[11]=view_[12]=zero_.data();return view_.data();}
private:std::vector<double> zero_;std::array<const double*,13> view_{};
};
struct ProbeConfig {unsigned frames=256,program=0,format=ENTROPY_PCM24_PACKED_LE;double frequency=1000.0,amplitude=0.01;unsigned firTaps=0,outputChannels=2;};
class FirstDacGraph final {
public:
 bool prepare(const ProbeConfig&);bool arm() noexcept;void disarm() noexcept {pipeline_.disarm();}
 bool produce(const double* const* eleven=nullptr) noexcept;
 DacPipeline& pipeline() noexcept {return pipeline_;}const ProbeConfig& config() const noexcept {return config_;}
 static EntropyHeadroomState probeLedger() noexcept;static bool configureProbeGraph(entropy::engine::AudioGraph&,unsigned program);
private:
 ProbeConfig config_{};entropy::engine::AudioGraph graph_;Input11 input_;entropy::engine::PlanarBuffer tone_,output_;
 DacPipeline pipeline_;std::array<const double*,2> selected_{};uint64_t sample_=0;bool ready_=false;
};
class SinkOwner final {
public:
 SinkOwner(DacPipeline& p,Sink& s):pipeline_(p),sink_(s){}~SinkOwner(){stop();}
 bool open(const EntropyDacSinkConfig&);bool start() noexcept;bool cycle() noexcept;void stop() noexcept;
private:DacPipeline& pipeline_;Sink& sink_;bool opened_=false,started_=false;
};
}
