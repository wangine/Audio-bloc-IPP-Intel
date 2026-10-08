#pragma once
#include "SpscAudioFifo.h"
#include "Engine/AudioGraph.h"
#include <EgressClockEngine.h>
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <entropy/dac_sink_abi.h>
#include <array>
#include <atomic>
#include <span>
#include <vector>

namespace entropy::egress_host {
enum class State : unsigned { Disarmed, Prepared, Armed, Preroll, Running, Stopping, Fault };
enum Fault : unsigned { None=0, Overflow=1, Starvation=2, Clock=4, Pcm=8, Device=16, Input=32 };
struct PipelineConfig {
    unsigned maximumFrames=1024, channels=2, fifoFrames=16384, targetFrames=2048;
    unsigned sinkRate=192000, format=ENTROPY_PCM24_PACKED_LE;
    bool guardianBypass=true;
};
// One instance per DAC. Producer owns publish(); DAC owner owns transfer/render.
// All prepare/recovery calls are quiescent, after both owners have joined.
class DacPipeline final {
public:
    bool prepare(const PipelineConfig&);
    bool arm(bool headroomReady, bool profileAllowed) noexcept;
    void disarm() noexcept;
    void fail(unsigned reason) noexcept;
    bool publish(const double* const*,unsigned frames) noexcept;
    bool transfer(unsigned frames) noexcept;
    unsigned feedUntilTarget(unsigned renderFrames) noexcept;
    bool render(unsigned frames,uint64_t position,uint64_t qpc100ns) noexcept;
    State state() const noexcept {return state_.load(std::memory_order_acquire);}
    unsigned faults() const noexcept {return faults_.load();}
    uint64_t xruns() const noexcept {return xruns_.load();}
    unsigned writable() const noexcept {return fifo_.writable();}
    unsigned readable() const noexcept {return fifo_.readable();}
    const PipelineConfig& config() const noexcept {return config_;}
    const EntropyEgressClockConfig& clockConfig() const noexcept {return clock_.config();}
    EntropyEgressClockTelemetry clockTelemetry() const noexcept {return clock_.telemetry();}
    EntropyPcm24Telemetry pcmTelemetry() const noexcept {return pcm_.telemetry();}
    const entropy::engine::PlanarBuffer& rendered() const noexcept {return rendered_;}
    std::span<const uint8_t> bytes(unsigned frames) const noexcept;
    static void guardianBypass(const double* const*,double* const*,unsigned channels,unsigned frames) noexcept;
private:
    bool active() const noexcept;
    void silence(unsigned frames) noexcept;
    PipelineConfig config_{};
    SpscAudioFifo fifo_;
    entropy::egress::EgressClockEngine clock_;
    entropy::pcm24::Pcm24ModuleEngine pcm_;
    entropy::engine::PlanarBuffer transfer_,rendered_;
    std::vector<uint8_t> pcmBytes_;
    std::atomic<State> state_{State::Disarmed};
    std::atomic<unsigned> faults_{None};
    std::atomic<uint64_t> xruns_{0};
    uint64_t nominalAtLastRender_=0; // DAC owner only
};

// Transport layout is exactly L/R/C then the eight CINEMA lanes, at 192 kHz.
// The two extra legacy Crossover cinema lanes are zero, never reordered.
class Input11 final {
public:
    void prepare(unsigned frames){zero_.assign(frames,0.0);}
    const double* const* map(const double* const* eleven) noexcept {
        if(!eleven) return nullptr;
        for(unsigned c=0;c<11;++c){if(!eleven[c])return nullptr;view_[c]=eleven[c];}
        view_[11]=view_[12]=zero_.data(); return view_.data();
    }
private:
    std::vector<double> zero_;std::array<const double*,13> view_{};
};
struct ProbeConfig {
    unsigned frames=256, program=0, format=ENTROPY_PCM24_PACKED_LE;
    double frequency=1000.0, amplitude=0.01;
    unsigned firTaps=0, outputChannels=2; // live FIR profiles are not implemented/approved
};
class FirstDacGraph final {
public:
    bool prepare(const ProbeConfig&);
    bool arm() noexcept;
    void disarm() noexcept {pipeline_.disarm();}
    bool produce(const double* const* eleven=nullptr) noexcept;
    DacPipeline& pipeline() noexcept {return pipeline_;}
    const ProbeConfig& config() const noexcept {return config_;}
    static EntropyHeadroomState probeLedger() noexcept;
    static bool configureProbeGraph(entropy::engine::AudioGraph&,unsigned program);
private:
    ProbeConfig config_{};
    entropy::engine::AudioGraph graph_;
    Input11 input_;
    entropy::engine::PlanarBuffer tone_,output_;
    DacPipeline pipeline_;
    std::array<const double*,2> selected_{};
    uint64_t sample_=0;
    bool ready_=false;
};

// Hardware and scripted fake sinks exercise the same lifecycle adapter.
class Sink {
public:
    virtual ~Sink()=default;
    virtual EntropyResult open(const EntropyDacSinkConfig&)=0;
    virtual EntropyResult start() noexcept=0;
    virtual EntropyResult wait(EntropyDacSinkBufferRequest&) noexcept=0;
    virtual EntropyResult submit(EntropyDacSinkPcmBlock&) noexcept=0;
    virtual EntropyDacSinkStatus status() const noexcept=0;
    virtual void stop() noexcept=0;
    virtual void close() noexcept=0;
};
class SinkOwner final {
public:
    SinkOwner(DacPipeline& p,Sink& s):pipeline_(p),sink_(s){}
    ~SinkOwner(){stop();} // Must be destroyed on the same DAC owner as open().
    bool open(const EntropyDacSinkConfig&); // DAC owner only
    bool start() noexcept;
    bool cycle() noexcept; // waits only here, never on DSP Graph owner
    void stop() noexcept;
private:
    DacPipeline& pipeline_;Sink& sink_;bool opened_=false,started_=false;
};
}
