#include "SourceDomainStream.h"
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <entropy/src_abi.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>

namespace entropy::host {
SourceDomainStream::~SourceDomainStream(){ stop(); }

bool SourceDomainStream::validConfig(const SourceDomainConfig& c) noexcept {
    return c.channels==2 && c.destinationRate==192000 && c.dspBlockFrames==1024 &&
           c.fifoCapacityFrames>=c.prerollFrames+c.dspBlockFrames &&
           c.prerollFrames>=c.dspBlockFrames &&
           c.captureFifoCapacityFrames>=c.endpointReadFrames*8u &&
           c.outputTargetFrames>=c.prerollFrames &&
           c.outputTargetFrames+c.dspBlockFrames<c.fifoCapacityFrames &&
           c.endpointReadFrames>=c.srcInputFrames && c.srcInputFrames>0;
}

bool SourceDomainStream::prepare(ModuleInstance& endpoint, ModuleInstance& src, const SourceDomainConfig& c) {
    if(running_.load() || !validConfig(c)) return false;
    EntropyEndpointApiV1 ep{}; ep.struct_size=sizeof(ep); std::string error;
    EntropySrcApiV1 sa{}; sa.struct_size=sizeof(sa);
    if(!endpoint.queryExtension(ENTROPY_ENDPOINT_EXTENSION_ID,ENTROPY_ENDPOINT_EXTENSION_VERSION,&ep,sizeof(ep),error) ||
       !src.queryExtension(ENTROPY_SRC_EXTENSION_ID,ENTROPY_SRC_EXTENSION_VERSION,&sa,sizeof(sa),error)) return false;
    if(!captureFifo_.prepare(c.channels,c.captureFifoCapacityFrames) ||
       !fifo_.prepare(c.channels,c.fifoCapacityFrames)) return false;
    endpoint_=&endpoint; src_=&src; config_=c; activeGeneration_=0;
    ready_=false; faults_=0; resets_=0; endpointRead_=0; srcIn_=0; srcOut_=0;
    endpointLost_=0; endpointAvailable_=0; peakLeft_=0.0; peakRight_=0.0;
    endpointLossWindow_.reset();
    signalFreshness_.reset();
    containerBits_=0; validBits_=0; discontinuityFlags_=0;
    return true;
}

bool SourceDomainStream::start() {
    if(!endpoint_||!src_||running_.exchange(true)) return false;
    stop_=false;
    try {
        captureWorker_=std::thread([this]{captureWorker();});
        srcWorker_=std::thread([this]{srcWorker();});
    } catch(...) {
        stop_=true;
        if(captureWorker_.joinable()) captureWorker_.join();
        if(srcWorker_.joinable()) srcWorker_.join();
        running_=false; return false;
    }
    return true;
}
void SourceDomainStream::stop() noexcept {
    stop_=true;

    // A provider may block read_source_f64 on an event-driven capture wait.
    // Cancel the one in-flight read before joining. The provider/library stays
    // alive until both workers have joined and disconnect has completed.
    if(endpoint_ && captureWorker_.joinable()) {
        EntropyEndpointCatalogApiV1 catalog{}; catalog.struct_size=sizeof(catalog);
        std::string error;
        if(endpoint_->queryExtension(ENTROPY_ENDPOINT_CATALOG_ID,
                                     ENTROPY_ENDPOINT_CATALOG_VERSION,
                                     &catalog,sizeof(catalog),error) &&
           catalog.cancel_read)
            catalog.cancel_read(endpoint_->handle());
    }

    if(captureWorker_.joinable()) captureWorker_.join();
    if(srcWorker_.joinable()) srcWorker_.join();
    running_=false; ready_=false;
}
bool SourceDomainStream::ready() const noexcept { return ready_.load(std::memory_order_acquire); }
bool SourceDomainStream::pop1024(double* const* outputs, SourceStreamDescriptor& descriptor) noexcept {
    if(!ready()) return false;
    if(!fifo_.pop(outputs,1024,descriptor)) return false;
    if(fifo_.telemetry().fill_frames<1024) ready_=false;
    return true;
}
SourceDomainTelemetry SourceDomainStream::telemetry() const noexcept {
    SourceDomainTelemetry t; t.capture_failed=stop_.load() && running_.load(); t.state=state_; t.source_rate=sourceRate_; t.channels=config_.channels;
    t.container_bits=containerBits_; t.valid_bits=validBits_;
    t.discontinuity_flags=discontinuityFlags_;
    t.src_engine=config_.srcEngine; t.src_latency_frames=latency_; t.generation=activeGeneration_;
    t.endpoint_frames_read=endpointRead_; t.src_input_frames=srcIn_; t.src_output_frames=srcOut_;
    t.endpoint_lost_frames=endpointLost_; t.endpoint_available_frames=endpointAvailable_;
    if (signalFreshness_.fresh()) { t.peak_left=peakLeft_; t.peak_right=peakRight_; }
    t.channel_peaks[0]=t.peak_left; t.channel_peaks[1]=t.peak_right;
    t.resets=resets_; t.faults=faults_; t.fifo=fifo_.telemetry();
    t.capture_fifo=captureFifo_.telemetry();
    t.preroll_frames=config_.prerollFrames; t.target_frames=config_.outputTargetFrames;
    return t;
}

void SourceDomainStream::captureWorker() noexcept {
    EntropyEndpointApiV1 ep{}; ep.struct_size=sizeof(ep); std::string error;
    if(!endpoint_->queryExtension(ENTROPY_ENDPOINT_EXTENSION_ID,ENTROPY_ENDPOINT_EXTENSION_VERSION,
                                  &ep,sizeof(ep),error) ||
       ep.connect(endpoint_->handle())!=ENTROPY_OK) {
        faults_.fetch_add(1); stop_=true; return;
    }
    EntropyEndpointStatus connectedStatus{};
    connectedStatus.struct_size = sizeof(connectedStatus);
    if (ep.get_status != nullptr &&
        ep.get_status(endpoint_->handle(), &connectedStatus) == ENTROPY_OK)
    {
        state_ = connectedStatus.state;
        sourceRate_ = connectedStatus.source_rate;
        containerBits_ = connectedStatus.container_bits;
        validBits_ = connectedStatus.valid_bits;
        discontinuityFlags_ = connectedStatus.discontinuity_flags;
        endpointLost_ = endpointLossWindow_.observe(
            connectedStatus.stream_generation, connectedStatus.lost_frames);
        endpointAvailable_ = connectedStatus.available_frames;
    }

    const auto cap=(std::max)(config_.endpointReadFrames,ep.minimum_buffer_frames);
    std::array<std::vector<double>,2> input{std::vector<double>(cap),std::vector<double>(cap)};
    std::array<const double*,2> pointers{input[0].data(),input[1].data()};
    SourceStreamDescriptor captureDescriptor{};
    std::uint32_t formatRevision=0;

    while(!stop_.load(std::memory_order_acquire)) {
        EntropyEndpointBlockF64 b{}; b.struct_size=sizeof(b); b.capacity_frames=cap;
        b.left=input[0].data(); b.right=input[1].data(); b.status.struct_size=sizeof(b.status);
        if(ep.read_source_f64(endpoint_->handle(),&b)!=ENTROPY_OK) {
            faults_.fetch_add(1); std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
        }
        state_=b.status.state; sourceRate_=b.status.source_rate;
        containerBits_=b.status.container_bits; validBits_=b.status.valid_bits;
        discontinuityFlags_=b.status.discontinuity_flags;
        endpointLost_=endpointLossWindow_.observe(
            b.status.stream_generation, b.status.lost_frames);
        endpointAvailable_=b.status.available_frames;
        if(b.frame_count==0 || b.status.source_rate==0 ||
           b.status.channels!=config_.channels) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
        }
        double pl=0.0,pr=0.0;
        for(std::uint32_t n=0;n<b.frame_count;++n) {
            pl=(std::max)(pl,std::abs(input[0][n])); pr=(std::max)(pr,std::abs(input[1][n]));
        }
        peakLeft_=pl; peakRight_=pr;
        signalFreshness_.captured();

        SourceStreamDescriptor descriptor{
            b.status.stream_generation,b.status.source_rate,b.status.channels,
            b.status.valid_bits,b.status.container_bits,0};
        const bool descriptorChanged =
            descriptor.generation!=captureDescriptor.generation ||
            descriptor.source_rate!=captureDescriptor.source_rate ||
            descriptor.channels!=captureDescriptor.channels ||
            descriptor.valid_bits!=captureDescriptor.valid_bits ||
            descriptor.container_bits!=captureDescriptor.container_bits;
        if(descriptorChanged) {
            descriptor.format_revision=++formatRevision;
            if(!captureFifo_.reset(descriptor)) {
                --formatRevision;
                std::this_thread::yield();
                continue;
            }
            captureDescriptor=descriptor;
        } else {
            descriptor.format_revision=captureDescriptor.format_revision;
        }
        endpointRead_.fetch_add(b.frame_count,std::memory_order_relaxed);
        if(!captureFifo_.push(pointers.data(),b.frame_count,descriptor)) {
            faults_.fetch_add(1,std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    ep.disconnect(endpoint_->handle());
}

void SourceDomainStream::srcWorker() noexcept {
    EntropySrcApiV1 sa{}; sa.struct_size=sizeof(sa); std::string error;
    if(!src_->queryExtension(ENTROPY_SRC_EXTENSION_ID,ENTROPY_SRC_EXTENSION_VERSION,
                             &sa,sizeof(sa),error)) {
        faults_.fetch_add(1); stop_=true; return;
    }

    std::array<std::vector<double>,2> input{
        std::vector<double>(config_.srcInputFrames),std::vector<double>(config_.srcInputFrames)};
    std::array<double*,2> captureOutputs{input[0].data(),input[1].data()};
    std::array<const double*,2> srcInputs{input[0].data(),input[1].data()};
    std::array<std::vector<double>,2> output;
    std::array<double*,2> srcOutputs{};
    bool configured=false; std::uint32_t outCap=0;
    SourceStreamDescriptor configuredDescriptor{};

    while(!stop_.load(std::memory_order_acquire)) {
        if(fifo_.telemetry().fill_frames>=config_.outputTargetFrames) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
        }

        SourceStreamDescriptor capturedDescriptor{};
        if(!captureFifo_.pop(captureOutputs.data(),config_.srcInputFrames,capturedDescriptor)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
        }
        if(capturedDescriptor.source_rate==0 ||
           capturedDescriptor.channels!=config_.channels) continue;

        if(!configured || configuredDescriptor!=capturedDescriptor) {
            EntropySrcConfig sc{}; sc.struct_size=sizeof(sc); sc.engine=config_.srcEngine;
            sc.source_rate=capturedDescriptor.source_rate;
            sc.destination_rate=config_.destinationRate;
            sc.channels=capturedDescriptor.channels;
            sc.maximum_input_frames=config_.srcInputFrames;
            if(sa.configure(src_->handle(),&sc)!=ENTROPY_OK) {
                faults_.fetch_add(1); std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
            }
            outCap=sc.required_output_capacity;
            try { output[0].assign(outCap,0.0); output[1].assign(outCap,0.0); }
            catch(...) { faults_.fetch_add(1); stop_=true; break; }
            srcOutputs={output[0].data(),output[1].data()};
            if(!fifo_.reset(capturedDescriptor)) {
                std::this_thread::yield();
                continue;
            }
            configuredDescriptor=capturedDescriptor; configured=true;
            activeGeneration_=capturedDescriptor.generation;
            ready_=false; resets_.fetch_add(1);
            latency_=0;
        }

        EntropySrcBlockF64 sb{}; sb.struct_size=sizeof(sb); sb.channels=2;
        sb.input_frames=config_.srcInputFrames; sb.output_capacity=outCap;
        sb.inputs=srcInputs.data(); sb.outputs=srcOutputs.data();
        if(sa.process(src_->handle(),&sb)!=ENTROPY_OK ||
           sb.consumed_frames!=config_.srcInputFrames) {
            faults_.fetch_add(1); continue;
        }
        std::uint32_t latency=0;
        if(sa.latency_output_frames &&
           sa.latency_output_frames(src_->handle(),&latency)==ENTROPY_OK)
            latency_=latency;
        srcIn_.fetch_add(sb.consumed_frames,std::memory_order_relaxed);
        srcOut_.fetch_add(sb.produced_frames,std::memory_order_relaxed);
        if(sb.produced_frames &&
           !fifo_.push(const_cast<const double* const*>(srcOutputs.data()),
                       sb.produced_frames,capturedDescriptor)) {
            faults_.fetch_add(1); ready_=false;
        }
        if(fifo_.telemetry().fill_frames>=config_.prerollFrames) ready_=true;
    }
}

} // namespace entropy::host

namespace entropy::host { bool SourceDomainStream::pop1024(double* const* outputs, std::uint64_t& generation) noexcept { SourceStreamDescriptor descriptor; if(!pop1024(outputs,descriptor)) return false; generation=descriptor.generation; return true; }}
