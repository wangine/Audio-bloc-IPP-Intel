#include "NativeEgress.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <windows.h>
#include <bcrypt.h>

namespace entropy::egress_host {
bool DacPipeline::active() const noexcept {auto s=state();return s==State::Armed||s==State::Preroll||s==State::Running;}
bool DacPipeline::prepare(const PipelineConfig& c) try {
    if(active() || c.sinkRate!=192000 || c.channels<1 || c.channels>16 ||
       c.maximumFrames<1 || c.maximumFrames>4096 || c.targetFrames<512 ||
       c.targetFrames+c.maximumFrames+100>=c.fifoFrames || !c.guardianBypass || c.format>1) return false;
    state_=State::Disarmed;
    if(!fifo_.prepare(c.channels,c.fifoFrames)) return false;
    EntropyEgressClockConfig cc{};cc.struct_size=sizeof(cc);cc.revision=1;
    cc.engine_rate=cc.sink_rate=192000;cc.channels=c.channels;
    cc.maximum_input_frames=cc.maximum_render_frames=c.maximumFrames;
    cc.fifo_capacity_frames=c.fifoFrames;cc.target_fill_frames=c.targetFrames;
    cc.maximum_correction_ppm=200;cc.correction_smoothing_ms=100;
    if(!clock_.configure(cc)) return false;
    EntropyPcm24Config pc{};pc.struct_size=sizeof(pc);pc.channels=c.channels;
    pc.maximum_frames=c.maximumFrames;pc.revision=1;pc.output_format=c.format;
    pc.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;pc.seed_mode=ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT;
    pc.reserved_u32=ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT;
    // Quiescent control-thread preparation, never the Graph/DAC callback.
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&pc.seed),sizeof(pc.seed),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0 || pc.seed==0) return false;
    if(!pcm_.configure(pc)) return false;
    metadataTransfer_.resize(c.maximumFrames);metadataRing_.resize(c.fifoFrames);
    transfer_.prepare(c.channels,c.maximumFrames);rendered_.prepare(c.channels,c.maximumFrames);
    pcmBytes_.assign(static_cast<size_t>(c.channels)*c.maximumFrames*(c.format?4:3),0);
    silentPrefix_=0;stream_={};transferred_=published_=0;haveStream_=contract_=false;lastRenderFirst_=0;config_=c;faults_=0;xruns_=0;nominalAtLastRender_=0;state_=State::Prepared;return true;
} catch(...) {fail(Input);return false;}
bool DacPipeline::prepareSilentCalibration() noexcept {
    if(state()!=State::Prepared)return false;
    transfer_.clear(config_.maximumFrames);
    for(unsigned remaining=config_.targetFrames;remaining;){const auto n=(std::min)(remaining,config_.maximumFrames);
        EntropyEgressClockPushBlockF64 b{};b.struct_size=sizeof(b);b.channels=config_.channels;b.frames=n;b.inputs=transfer_.readPointers();
        if(clock_.push(b)!=ENTROPY_OK||b.consumed_frames!=n)return false;remaining-=n;silentPrefix_+=n;}
    return true;
}
void DacPipeline::silencePrefix(unsigned frames,unsigned prefix) noexcept {
    if(frames<=config_.maximumFrames&&prefix<=frames)std::fill_n(pcmBytes_.data(),prefix*config_.channels*(config_.format?4:3),uint8_t{0});
}
bool DacPipeline::renderOffset(unsigned frames,unsigned offset,uint64_t p,uint64_t q) noexcept {
    if(offset>=frames)return false;
    if(!render(frames-offset,p,q))return false;
    const auto stride=config_.channels*(config_.format?4:3);
    std::memmove(pcmBytes_.data()+offset*stride,pcmBytes_.data(),(frames-offset)*stride);
    std::fill_n(pcmBytes_.data(),offset*stride,uint8_t{0});return true;
}
bool DacPipeline::arm(bool policy,bool allowed) noexcept {
    if(!policy||!allowed||state()!=State::Prepared) return false;
    state_=State::Armed;return true;
}
void DacPipeline::disarm() noexcept {
    auto current=state();
    while(current!=State::Fault && !state_.compare_exchange_weak(current,State::Disarmed,std::memory_order_acq_rel)){}
}
void DacPipeline::fail(unsigned f) noexcept {faults_.fetch_or(f);state_.store(State::Fault,std::memory_order_release);}
bool DacPipeline::publish(const double* const* src,unsigned n) noexcept {
    GraphInterval v{0,published_,0,n};return publish(src,n,v);
}
bool DacPipeline::publish(const double* const* src,unsigned n,GraphInterval interval) noexcept {
    if(!active() || !n || n>config_.maximumFrames) return false;
    if(!src){fail(Input);return false;}
    for(unsigned c=0;c<config_.channels;++c){if(!src[c]){fail(Input);return false;}
        for(unsigned f=0;f<n;++f) if(!std::isfinite(src[c][f])){fail(Input);return false;}}
    if(!fifo_.push(src,n,interval)){xruns_.fetch_add(1);fail(Overflow);return false;}published_+=n;return true;
}
bool DacPipeline::transfer(unsigned n) noexcept {
    if(!active() || !n || n>config_.maximumFrames) return false;
    GraphInterval interval{};
    if(!fifo_.pop(transfer_.writePointers(),n,&interval,metadataTransfer_.data())){xruns_.fetch_add(1);fail(Starvation);return false;}
    if(!haveStream_){stream_=interval;stream_.frames=0;haveStream_=true;}
    if(interval.epoch!=stream_.epoch||interval.firstGraphFrame!=stream_.firstGraphFrame+transferred_){fail(Clock);return false;}
    for(unsigned f=0;f<n;++f)metadataRing_[(transferred_+f)&(config_.fifoFrames-1)]=metadataTransfer_[f];
    transferred_+=n;stream_.capture100ns=interval.capture100ns;
    EntropyEgressClockPushBlockF64 b{};b.struct_size=sizeof(b);b.channels=config_.channels;
    b.frames=n;b.inputs=transfer_.readPointers();
    if(clock_.push(b)!=ENTROPY_OK || b.consumed_frames!=n){fail(Clock);return false;}return true;
}
bool DacPipeline::primeGraphPosition(double first,unsigned frames) noexcept {
    // Select from the shared graph timeline, including the required sinc history.
    if(!haveStream_){if(!readable()||!transfer((std::min)(readable(),config_.maximumFrames)))return false;}
    const double offset=first-double(stream_.firstGraphFrame)+silentPrefix_;
    if(!std::isfinite(offset)||offset<48)return false;
    const double required=offset+(std::max)(config_.targetFrames,frames+100);
    for(unsigned k=0;k<64&&double(clock_.telemetry().nominal_frames)<required;++k){
        const unsigned missing=static_cast<unsigned>(std::ceil(required-double(clock_.telemetry().nominal_frames)));
        const auto n=(std::min)({readable(),config_.maximumFrames,missing});if(!n||!transfer(n))return false;
    }
    if(!clock_.primeNominalPosition(offset,frames))return false;
    contract_=true;return true;
}
unsigned DacPipeline::feedUntilTarget(unsigned frames) noexcept {
    unsigned moved=0;
    // Finite work bound even if a concurrent producer keeps publishing.
    for(unsigned k=0;k<64 && active();++k){
        const auto t=clock_.telemetry();
        const double fill=contract_ ? double(t.nominal_frames)-clock_.nominalReadPosition() : t.output_frames==0 ? double(t.nominal_frames)
            : t.fill_frames+double(t.nominal_frames-nominalAtLastRender_);
        const unsigned target=t.state==ENTROPY_EGRESS_CLOCK_PREROLL ? config_.targetFrames+frames+100
            : (std::max)(config_.targetFrames,frames+100);
        if(fill>=target) break;
        auto available=fifo_.readable();if(!available) break;
        auto n=(std::min)(available,config_.maximumFrames);
        if(contract_)n=(std::min)(n,static_cast<unsigned>(std::ceil(target-fill)));
        if(!transfer(n)) break;moved+=n;
    }
    return moved;
}
void DacPipeline::guardianBypass(const double* const* src,double* const* dst,unsigned channels,unsigned frames) noexcept {
    for(unsigned c=0;c<channels;++c) if(src[c]!=dst[c]) std::memcpy(dst[c],src[c],static_cast<size_t>(frames)*sizeof(double));
}
std::span<const uint8_t> DacPipeline::bytes(unsigned n) const noexcept {
    if(n>config_.maximumFrames) return {};
    const auto size=static_cast<size_t>(n)*config_.channels*(config_.format?4:3);
    if(size>pcmBytes_.size())return {};
    return {pcmBytes_.data(),size};
}
void DacPipeline::silence(unsigned n) noexcept {
    if(n>config_.maximumFrames)return;
    rendered_.clear(n);std::fill_n(pcmBytes_.data(),bytes(n).size(),uint8_t{0});
}
bool DacPipeline::render(unsigned n,uint64_t pos,uint64_t time) noexcept {
    if(pcmBytes_.empty())return false;
    if(!n||n>config_.maximumFrames){fail(Device);return false;}
    silence(n);
    if(!active()) return false;
    EntropyEgressClockRenderBlockF64 b{};b.struct_size=sizeof(b);b.channels=config_.channels;
    b.frames=n;b.outputs=rendered_.writePointers();b.sink_position_frames=pos;b.host_time_100ns=time;
    const auto previousResync=clock_.telemetry().resync_count;
    lastRenderFirst_=graphReadPosition();
    const auto result=clock_.render(b);nominalAtLastRender_=clock_.telemetry().nominal_frames;
    if(contract_&&clock_.telemetry().resync_count!=previousResync){fail(Clock);silence(n);return false;}
    if(result!=ENTROPY_OK || b.produced_frames!=n || b.state==ENTROPY_EGRESS_CLOCK_FAULT){fail(Clock);silence(n);return false;}
    if(b.state==ENTROPY_EGRESS_CLOCK_STARVED){xruns_.fetch_add(1);fail(Starvation);silence(n);return false;}
    // Optional Guardian is the exact same FP64 view, no phase kernel or gain.
    for(unsigned c=0;c<config_.channels;++c) for(unsigned f=0;f<n;++f) {
        const double v=rendered_.readChannel(c)[f];
        if(!std::isfinite(v)){fail(Pcm);silence(n);return false;}
    }
    EntropyPcm24BlockF64 p{};p.struct_size=sizeof(p);p.channels=config_.channels;p.frames=n;
    p.inputs=rendered_.readPointers();p.output_interleaved=pcmBytes_.data();p.output_capacity_bytes=static_cast<unsigned>(bytes(n).size());
    if(pcm_.process(p)!=ENTROPY_OK || p.produced_bytes!=bytes(n).size()){fail(Pcm);silence(n);return false;}
    State expected=state();
    if(expected==State::Disarmed || expected==State::Fault){silence(n);return false;}
    state_.compare_exchange_strong(expected,b.state==ENTROPY_EGRESS_CLOCK_LOCKED?State::Running:State::Preroll);
    if(!active()){silence(n);return false;}return true;
}
EntropyHeadroomState FirstDacGraph::probeLedger() noexcept {
    EntropyHeadroomState s{};s.struct_size=sizeof(s);s.revision=1;s.group_count=1;s.active_channel_mask=0xffff;
    s.transition_ms=1.0;s.groups[0].struct_size=sizeof(s.groups[0]);s.groups[0].bounded=1;
    s.groups[0].declared_positive_contribution_db=0;s.groups[0].guaranteed_input_headroom_db=40;s.groups[0].preventive_reserve_db=6;
    return s;
}
bool FirstDacGraph::configureProbeGraph(entropy::engine::AudioGraph& g,unsigned program) {
    if(program>1 || !g.setOfflineParameter("cinemaSource",program) || !g.applyOfflineParameters())return false;
    for(unsigned ch=1;ch<=16;++ch){auto set=[&](const char* key,double v){return g.setOfflineParameter((std::string(key)+std::to_string(ch)).c_str(),v);};
        for(auto key:{"hpOn","lpOn","allPass","processMode","mute","emergencyMute","protectorOn","musicGainDb","cinemaGainDb","musicDelayMs","cinemaDelayMs"}) if(!set(key,0))return false;
        double role=ch==1?1:ch==2?6:0;if(!set("source",role)||!set("cinemaRole",role))return false;
    }
    for(unsigned ch=1;ch<=3;++ch)if(!g.setOfflineParameter(("musicTrimDb"+std::to_string(ch)).c_str(),0))return false;
    for(unsigned ch=1;ch<=10;++ch)if(!g.setOfflineParameter(("cinemaTrimDb"+std::to_string(ch)).c_str(),0))return false;
    return g.applyOfflineParameters(true);
}
bool FirstDacGraph::prepare(const ProbeConfig& c) try {
    ready_=false;
    if(pipeline_.state()==State::Running||pipeline_.state()==State::Preroll||pipeline_.state()==State::Armed)return false;
    if(c.program>1 || c.frames<128 || c.frames>1024 || c.firTaps || c.outputChannels!=2 ||
       !std::isfinite(c.amplitude)||c.amplitude<=0||c.amplitude>0.01||
       !std::isfinite(c.frequency)||c.frequency<10||c.frequency>20000||c.format>1)return false;
    if(!graph_.prepare(c.frames) || !configureProbeGraph(graph_,c.program) ||
       !graph_.applyEgressHeadroomPolicy(probeLedger(),20.0)) return false;
    tone_.prepare(11,c.frames);output_.prepare(16,c.frames);input_.prepare(c.frames);tone_.clear(c.frames);
    // Settle the declared 1 ms Headroom ramp off-thread, with zero input.
    for(unsigned samples=0;samples<2048;samples+=c.frames)
        if(graph_.process(input_.map(tone_.readPointers()),output_.writePointers(),c.frames)!=ENTROPY_OK)return false;
    PipelineConfig pc;pc.maximumFrames=1024;pc.format=c.format;
    if(!pipeline_.prepare(pc))return false;
    config_=c;sample_=0;ready_=true;return true;
} catch(...) {ready_=false;pipeline_.fail(Input);return false;}
bool FirstDacGraph::arm() noexcept {return ready_ && pipeline_.arm(graph_.egressPolicyReady(),config_.firTaps==0&&config_.outputChannels==2);}
bool FirstDacGraph::produce(const double* const* eleven) noexcept {
    auto state=pipeline_.state();if(state!=State::Armed&&state!=State::Preroll&&state!=State::Running)return false;
    if(!eleven){tone_.clear(config_.frames);for(unsigned f=0;f<config_.frames;++f){
        double v=config_.amplitude*std::sin(6.283185307179586476925286766559*config_.frequency*double(sample_+f)/192000.0);
        // L/R/C then CINEMA L/R/C/LFE/SL/SR/BL/BR; no source SRC.
        tone_.channel(config_.program?3:0)[f]=v;tone_.channel(config_.program?4:1)[f]=v;
    }eleven=tone_.readPointers();sample_+=config_.frames;}
    for(unsigned c=0;c<11;++c){if(!eleven[c]){pipeline_.fail(Input);return false;}
        for(unsigned f=0;f<config_.frames;++f) if(!std::isfinite(eleven[c][f])||std::abs(eleven[c][f])>0.01){pipeline_.fail(Input);return false;}}
    auto inputs=input_.map(eleven);
    if(!inputs||graph_.process(inputs,output_.writePointers(),config_.frames)!=ENTROPY_OK){pipeline_.fail(Input);return false;}
    selected_[0]=output_.readChannel(0);selected_[1]=output_.readChannel(1);
    return pipeline_.publish(selected_.data(),config_.frames);
}
bool SinkOwner::open(const EntropyDacSinkConfig& c) try {
    if(opened_ || c.sample_rate!=192000 || c.channels!=pipeline_.config().channels || c.output_format!=pipeline_.config().format ||
       pipeline_.state()!=State::Armed)return false;
    if(sink_.open(c)!=ENTROPY_OK){pipeline_.fail(Device);sink_.close();return false;}
    auto status=sink_.status();
    if(!status.buffer_frames || status.buffer_frames>pipeline_.config().maximumFrames){pipeline_.fail(Device);sink_.close();return false;}
    opened_=true;return true;
} catch(...) {pipeline_.fail(Device);sink_.close();opened_=false;return false;}
bool SinkOwner::start() noexcept {
    if(!opened_ || started_ || pipeline_.state()!=State::Armed)return false;
    pipeline_.feedUntilTarget(sink_.status().buffer_frames);
    if(pipeline_.clockTelemetry().nominal_frames<pipeline_.config().targetFrames+sink_.status().buffer_frames+48)return false;
    if(sink_.start()!=ENTROPY_OK){pipeline_.fail(Device);return false;}started_=true;return true;
}
bool SinkOwner::cycle() noexcept {
    if(!started_)return false;
    EntropyDacSinkBufferRequest req{};req.struct_size=sizeof(req);req.timeout_ms=100;
    if(sink_.wait(req)!=ENTROPY_OK){pipeline_.fail(Device);stop();return false;}
    if(req.frames>pipeline_.config().maximumFrames||!req.frames||req.bytes!=pipeline_.bytes(req.frames).size()){pipeline_.fail(Device);stop();return false;}
    pipeline_.feedUntilTarget(req.frames);
    const bool good=pipeline_.render(req.frames,req.device_position_frames,req.device_qpc_position);
    auto bytes=pipeline_.bytes(req.frames);
    EntropyDacSinkPcmBlock block{};block.struct_size=sizeof(block);block.frames=req.frames;
    block.data=bytes.data();block.byte_count=static_cast<unsigned>(bytes.size());
    if(sink_.submit(block)!=ENTROPY_OK || block.accepted_frames!=req.frames){pipeline_.fail(Device);stop();return false;}
    if(!good){stop();return false;}return true;
}
void SinkOwner::stop() noexcept {
    if(started_)sink_.stop();if(opened_)sink_.close();started_=opened_=false;
    if(pipeline_.state()!=State::Fault)pipeline_.disarm();
}
}
