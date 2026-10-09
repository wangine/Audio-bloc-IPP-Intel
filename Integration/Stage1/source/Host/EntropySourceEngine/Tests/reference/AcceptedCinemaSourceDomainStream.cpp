#include "CinemaSourceDomainStream.h"

#include <entropy/cinema_endpoint_abi.h>
#include <entropy/src_abi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

namespace entropy::host {

CinemaSourceDomainStream::~CinemaSourceDomainStream()
{
    stop();
}

bool CinemaSourceDomainStream::validConfig(const SourceDomainConfig& c) noexcept
{
    return c.channels == 8 && c.destinationRate == 192000 &&
           c.dspBlockFrames == 1024 &&
           c.fifoCapacityFrames >= c.prerollFrames + c.dspBlockFrames &&
           c.prerollFrames >= c.dspBlockFrames &&
           c.captureFifoCapacityFrames >= c.endpointReadFrames * 8u &&
           c.outputTargetFrames >= c.prerollFrames &&
           c.outputTargetFrames + c.dspBlockFrames < c.fifoCapacityFrames &&
           c.endpointReadFrames <= c.dspBlockFrames &&
           c.endpointReadFrames >= c.srcInputFrames && c.srcInputFrames > 0;
}

bool CinemaSourceDomainStream::prepare(ModuleInstance& endpoint,
                                       ModuleInstance& src,
                                       const SourceDomainConfig& c)
{
    if (running_.load() || !validConfig(c)) return false;

    EntropyCinemaEndpointApiV1 endpointApi{};
    endpointApi.struct_size = sizeof(endpointApi);
    EntropySrcApiV1 srcApi{};
    srcApi.struct_size = sizeof(srcApi);
    std::string error;
    if (!endpoint.queryExtension(ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID,
                                 ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION,
                                 &endpointApi, sizeof(endpointApi), error) ||
        !src.queryExtension(ENTROPY_SRC_EXTENSION_ID,
                            ENTROPY_SRC_EXTENSION_VERSION,
                            &srcApi, sizeof(srcApi), error) ||
        endpointApi.maximum_channels < c.channels ||
        !captureFifo_.prepare(c.channels, c.captureFifoCapacityFrames) ||
        !fifo_.prepare(c.channels, c.fifoCapacityFrames))
        return false;

    endpoint_ = &endpoint;
    src_ = &src;
    config_ = c;
    publishedChannels_ = c.channels;
    publishedSrcEngine_ = c.srcEngine;
    requestedSrcEngine_ = c.srcEngine;
    activeGeneration_ = 0;
    ready_ = false;
    endpointRead_ = 0;
    srcIn_ = 0;
    srcOut_ = 0;
    resets_ = 0;
    faults_ = 0;
    sourceRate_ = 0;
    state_ = 0;
    latency_ = 0;
    endpointAvailable_ = 0;
    validBits_ = 0;
    for (auto& peak : peaks_) peak = 0.0;
    signalFreshness_.reset();
    return true;
}

bool CinemaSourceDomainStream::start()
{
    if (!endpoint_ || !src_ || running_.exchange(true)) return false;
    stop_ = false;
    try
    {
        captureWorker_ = std::thread([this] { captureWorker(); });
        srcWorker_ = std::thread([this] { srcWorker(); });
    }
    catch (...)
    {
        stop_ = true;
        if (captureWorker_.joinable()) captureWorker_.join();
        if (srcWorker_.joinable()) srcWorker_.join();
        running_ = false;
        return false;
    }
    return true;
}

void CinemaSourceDomainStream::stop() noexcept
{
    stop_ = true;
    if (captureWorker_.joinable()) captureWorker_.join();
    if (srcWorker_.joinable()) srcWorker_.join();
    running_ = false;
    ready_ = false;
}

bool CinemaSourceDomainStream::requestSrcEngine(std::uint32_t engine) noexcept
{
    if (!running_.load(std::memory_order_acquire) || stop_.load()) return false;
    requestedSrcEngine_.store(engine, std::memory_order_release);
    return true;
}

bool CinemaSourceDomainStream::ready() const noexcept
{
    return ready_.load(std::memory_order_acquire);
}

bool CinemaSourceDomainStream::pop1024(double* const* outputs, SourceStreamDescriptor& descriptor) noexcept
{
    if (!ready()) return false;
    if (!fifo_.pop(outputs, 1024, descriptor)) return false;
    if (fifo_.telemetry().fill_frames < 1024) ready_ = false;
    return true;
}

SourceDomainTelemetry CinemaSourceDomainStream::telemetry() const noexcept
{
    SourceDomainTelemetry result;
    result.state = state_;
    result.source_rate = sourceRate_;
    result.channels = publishedChannels_;
    result.container_bits = validBits_;
    result.valid_bits = validBits_;
    result.src_engine = publishedSrcEngine_;
    result.src_latency_frames = latency_;
    result.generation = activeGeneration_;
    result.endpoint_frames_read = endpointRead_;
    result.src_input_frames = srcIn_;
    result.src_output_frames = srcOut_;
    result.endpoint_available_frames = endpointAvailable_;
    if (signalFreshness_.fresh())
    {
        result.peak_left = peaks_[0];
        result.peak_right = peaks_[1];
        for (std::size_t channel = 0; channel < result.channel_peaks.size(); ++channel)
            result.channel_peaks[channel] = peaks_[channel];
    }
    result.resets = resets_;
    result.faults = faults_;
    result.fifo = fifo_.telemetry();
    result.capture_fifo = captureFifo_.telemetry();
    result.preroll_frames = config_.prerollFrames;
    result.target_frames = config_.outputTargetFrames;
    return result;
}

void CinemaSourceDomainStream::captureWorker() noexcept
{
    EntropyCinemaEndpointApiV1 endpointApi{};
    endpointApi.struct_size = sizeof(endpointApi);
    std::string error;
    if (!endpoint_->queryExtension(ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID,
                                   ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION,
                                   &endpointApi, sizeof(endpointApi), error))
    {
        faults_.fetch_add(1);
        stop_ = true;
        return;
    }

    const auto capacity = (std::max)(config_.endpointReadFrames,
                                     endpointApi.minimum_buffer_frames);
    std::array<std::vector<double>, 8> input;
    std::array<double*, 8> pointers{};
    std::array<const double*, 8> constPointers{};
    for (std::size_t channel = 0; channel < input.size(); ++channel)
    {
        input[channel].resize(capacity);
        pointers[channel] = input[channel].data();
        constPointers[channel] = input[channel].data();
    }
    SourceStreamDescriptor captureDescriptor{};
    std::uint32_t formatRevision = 0;

    // This worker owns endpoint handles. A publisher may appear after Host startup.
    // Retry on this non-render thread; never terminate SRC merely for absence.
    bool connected = false;
    auto lastActive = std::chrono::steady_clock::now();
    const auto retryPause = [&] {
        for (unsigned n = 0; n < 10 && !stop_.load(std::memory_order_acquire); ++n)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };
    const auto disconnected = [&] {
        state_ = ENTROPY_ENDPOINT_DISCONNECTED;
        sourceRate_ = 0;
        endpointAvailable_ = 0;
        validBits_ = 0;
        // SRC and pop1024 retain ownership of readiness. Existing queued frames
        // must remain drainable so a full FIFO cannot block the new descriptor.
    };
    while (!stop_.load(std::memory_order_acquire))
    {
        if (!connected)
        {
            if (endpointApi.connect(endpoint_->handle()) != ENTROPY_OK)
            {
                faults_.fetch_add(1, std::memory_order_relaxed);
                disconnected();
                retryPause();
                continue;
            }
            connected = true;
            captureDescriptor = {}; // Reopened publishers can reuse generation numbers.
            lastActive = std::chrono::steady_clock::now();
        }
        EntropyCinemaEndpointBlockF64 block{};
        block.struct_size = sizeof(block);
        block.capacity_frames = capacity;
        block.output_capacity_channels = config_.channels;
        block.outputs = pointers.data();
        if (endpointApi.read_source_f64(endpoint_->handle(), &block) != ENTROPY_OK)
        {
            faults_.fetch_add(1, std::memory_order_relaxed);
            endpointApi.disconnect(endpoint_->handle());
            connected = false;
            disconnected();
            retryPause();
            continue;
        }
        // The reader's mapping keeps an exited publisher's old object alive.
        // Reopen after sustained inactivity to discover the new publisher object.
        const auto now = std::chrono::steady_clock::now();
        if (block.status.state == ENTROPY_ENDPOINT_ACTIVE) lastActive = now;
        if (block.status.state == ENTROPY_ENDPOINT_DISCONNECTED ||
            now - lastActive >= std::chrono::seconds(1))
        {
            endpointApi.disconnect(endpoint_->handle());
            connected = false;
            disconnected();
            retryPause();
            continue;
        }

        state_ = block.status.state;
        sourceRate_ = block.status.source_rate;
        endpointAvailable_ = block.status.available_frames;
        validBits_ = block.status.sample_bits;
        if (block.frame_count == 0 || block.status.source_rate == 0 ||
            block.status.channels < config_.channels)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        for (std::size_t channel = 0; channel < input.size(); ++channel)
        {
            double peak = 0.0;
            for (std::uint32_t frame = 0; frame < block.frame_count; ++frame)
                peak = (std::max)(peak, std::abs(input[channel][frame]));
            peaks_[channel] = peak;
        }

        signalFreshness_.captured();
        SourceStreamDescriptor descriptor{
            block.status.stream_generation, block.status.source_rate,
            block.status.channels, block.status.sample_bits,
            block.status.sample_bits, 0};
        const bool descriptorChanged =
            descriptor.generation != captureDescriptor.generation ||
            descriptor.source_rate != captureDescriptor.source_rate ||
            descriptor.channels != captureDescriptor.channels ||
            descriptor.valid_bits != captureDescriptor.valid_bits ||
            descriptor.container_bits != captureDescriptor.container_bits;
        if (descriptorChanged)
        {
            descriptor.format_revision = ++formatRevision;
            if (!captureFifo_.reset(descriptor))
            {
                --formatRevision;
                std::this_thread::yield();
                continue;
            }
            captureDescriptor = descriptor;
        }
        else descriptor.format_revision = captureDescriptor.format_revision;
        endpointRead_.fetch_add(block.frame_count, std::memory_order_relaxed);
        if (!captureFifo_.push(constPointers.data(), block.frame_count, descriptor))
        {
            faults_.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (connected) endpointApi.disconnect(endpoint_->handle());
}

void CinemaSourceDomainStream::srcWorker() noexcept
{
    EntropySrcApiV1 srcApi{};
    srcApi.struct_size = sizeof(srcApi);
    std::string error;
    if (!src_->queryExtension(ENTROPY_SRC_EXTENSION_ID,
                              ENTROPY_SRC_EXTENSION_VERSION,
                              &srcApi, sizeof(srcApi), error))
    {
        faults_.fetch_add(1);
        stop_ = true;
        return;
    }

    std::array<std::vector<double>, 8> input;
    std::array<double*, 8> captureOutputs{};
    std::array<const double*, 8> srcInputs{};
    std::array<std::vector<double>, 8> output;
    std::array<double*, 8> srcOutputs{};
    for (std::size_t channel = 0; channel < input.size(); ++channel)
    {
        input[channel].resize(config_.srcInputFrames);
        captureOutputs[channel] = input[channel].data();
        srcInputs[channel] = input[channel].data();
    }

    bool configured = false;
    std::uint32_t configuredEngine = config_.srcEngine;
    std::uint32_t outputCapacity = 0;
    SourceStreamDescriptor configuredDescriptor{};

    while (!stop_.load(std::memory_order_acquire))
    {
        if (fifo_.telemetry().fill_frames >= config_.outputTargetFrames)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        SourceStreamDescriptor capturedDescriptor{};
        if (!captureFifo_.pop(captureOutputs.data(), config_.srcInputFrames,
                              capturedDescriptor))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (capturedDescriptor.source_rate == 0 ||
            capturedDescriptor.channels != config_.channels) continue;

        const auto requestedEngine = requestedSrcEngine_.load(std::memory_order_acquire);
        const bool sourceBoundary = !configured ||
                                    configuredDescriptor != capturedDescriptor;
        if (sourceBoundary || configuredEngine != requestedEngine)
        {
            EntropySrcConfig config{};
            config.struct_size = sizeof(config);
            config.engine = requestedEngine;
            config.source_rate = capturedDescriptor.source_rate;
            config.destination_rate = config_.destinationRate;
            config.channels = capturedDescriptor.channels;
            config.maximum_input_frames = config_.srcInputFrames;
            if (srcApi.configure(src_->handle(), &config) != ENTROPY_OK)
            {
                faults_.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            outputCapacity = config.required_output_capacity;
            try
            {
                for (std::size_t channel = 0; channel < output.size(); ++channel)
                {
                    output[channel].assign(outputCapacity, 0.0);
                    srcOutputs[channel] = output[channel].data();
                }
            }
            catch (...)
            {
                faults_.fetch_add(1);
                stop_ = true;
                break;
            }
            // An algorithm change retains queued frames. A source descriptor
            // boundary invalidates converted frames from the previous epoch.
            if (sourceBoundary)
            {
                if (!fifo_.reset(capturedDescriptor))
                {
                    std::this_thread::yield();
                    continue;
                }
                ready_ = false;
            }
            configuredDescriptor = capturedDescriptor;
            configuredEngine = requestedEngine;
            configured = true;
            activeGeneration_ = capturedDescriptor.generation;
            resets_.fetch_add(1);
            latency_ = 0;
            publishedSrcEngine_.store(configuredEngine, std::memory_order_release);
        }

        EntropySrcBlockF64 block{};
        block.struct_size = sizeof(block);
        block.channels = config_.channels;
        block.input_frames = config_.srcInputFrames;
        block.output_capacity = outputCapacity;
        block.inputs = srcInputs.data();
        block.outputs = srcOutputs.data();
        if (srcApi.process(src_->handle(), &block) != ENTROPY_OK ||
            block.consumed_frames != config_.srcInputFrames)
        {
            faults_.fetch_add(1);
            continue;
        }
        std::uint32_t latency = 0;
        if (srcApi.latency_output_frames &&
            srcApi.latency_output_frames(src_->handle(), &latency) == ENTROPY_OK)
            latency_ = latency;
        srcIn_.fetch_add(block.consumed_frames, std::memory_order_relaxed);
        srcOut_.fetch_add(block.produced_frames, std::memory_order_relaxed);
        if (block.produced_frames &&
            !fifo_.push(const_cast<const double* const*>(srcOutputs.data()),
                        block.produced_frames, capturedDescriptor))
        {
            faults_.fetch_add(1);
            ready_ = false;
        }
        if (fifo_.telemetry().fill_frames >= config_.prerollFrames)
            ready_ = true;
    }
}

} // namespace entropy::host

namespace entropy::host { bool CinemaSourceDomainStream::pop1024(double* const* outputs, std::uint64_t& generation) noexcept { SourceStreamDescriptor descriptor; if(!pop1024(outputs,descriptor)) return false; generation=descriptor.generation; return true; }}
