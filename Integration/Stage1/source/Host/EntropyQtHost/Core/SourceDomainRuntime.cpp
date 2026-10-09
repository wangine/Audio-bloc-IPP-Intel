#include "SourceDomainRuntime.h"
#include "RealtimeBufferPolicy.h"

#include <thread>

namespace entropy::host {

SourceDomainRuntime::~SourceDomainRuntime()
{
    stop();
}

bool SourceDomainRuntime::prepareOne(SourceDomainStream& stream,
                                     SourceModuleResolver& runtime,
                                     const char* endpointId,
                                     const char* srcId,
                                     std::uint32_t engine)
{
    auto* endpoint = runtime.findInstance(endpointId);
    auto* src = runtime.findInstance(srcId);
    if (!endpoint || !src) return false;

    SourceDomainConfig config;
    config.channels = 2;
    config.destinationRate = 192000;
    config.dspBlockFrames = 1024;
    config.fifoCapacityFrames = RealtimeBufferPolicy::sourceCapacityFrames;
    config.prerollFrames = RealtimeBufferPolicy::sourcePrerollFrames;
    config.captureFifoCapacityFrames =
        RealtimeBufferPolicy::sourceCaptureCapacityFrames;
    config.outputTargetFrames = RealtimeBufferPolicy::sourceTargetFrames;
    config.srcEngine = engine;
    return stream.prepare(*endpoint, *src, config);
}

SourceDomainRuntime::MusicSlot* SourceDomainRuntime::acquireMusic() const noexcept
{
    musicReaders_.fetch_add(1, std::memory_order_acq_rel);
    return activeMusic_.load(std::memory_order_acquire);
}

void SourceDomainRuntime::releaseMusic() const noexcept
{
    musicReaders_.fetch_sub(1, std::memory_order_acq_rel);
}

bool SourceDomainRuntime::waitForMusicReaders(
    std::chrono::milliseconds timeout) const noexcept
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (musicReaders_.load(std::memory_order_acquire) != 0)
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

bool SourceDomainRuntime::prepareMusic(SourceModuleResolver* initialMusicRuntime,
                                       SourceModuleResolver& auxRuntime,
                                       SourceModuleResolver* cinemaRuntime,
                                       std::uint32_t musicEngine,
                                       std::uint32_t auxEngine,
                                       std::uint32_t cinemaEngine)
{
    stop();
    auxRuntime_ = &auxRuntime;
    cinemaRuntime_ = cinemaRuntime;
    auxEngine_ = auxEngine;
    cinemaEngine_ = cinemaEngine;

    auxPrepared_ = prepareOne(
        aux_, auxRuntime, "endpoint.aux", "src.aux", auxEngine);
    if (!auxPrepared_) return false;

    cinemaPrepared_ = false;
    if (cinemaRuntime_ != nullptr)
    {
        auto* cinemaEndpoint = cinemaRuntime_->findInstance("endpoint.cinema");
        auto* cinemaSrc = cinemaRuntime_->findInstance("src.cinema");
        if (cinemaEndpoint && cinemaSrc)
        {
            SourceDomainConfig cinemaConfig;
            cinemaConfig.channels = 8;
            cinemaConfig.destinationRate = 192000;
            cinemaConfig.dspBlockFrames = 1024;
            cinemaConfig.fifoCapacityFrames = RealtimeBufferPolicy::sourceCapacityFrames;
            cinemaConfig.prerollFrames = RealtimeBufferPolicy::sourcePrerollFrames;
            cinemaConfig.captureFifoCapacityFrames =
                RealtimeBufferPolicy::sourceCaptureCapacityFrames;
            cinemaConfig.outputTargetFrames = RealtimeBufferPolicy::sourceTargetFrames;
            cinemaConfig.endpointReadFrames = 1024;
            cinemaConfig.srcEngine = cinemaEngine;
            cinemaPrepared_ = cinema_.prepare(
                *cinemaEndpoint, *cinemaSrc, cinemaConfig);
        }
    }

    if (initialMusicRuntime != nullptr)
    {
        auto slot = std::make_unique<MusicSlot>();
        slot->runtime = initialMusicRuntime;
        slot->engine = musicEngine;
        if (!prepareOne(slot->stream, *slot->runtime,
                        "endpoint.tidal", "src.tidal", musicEngine))
            return false;

        std::lock_guard<std::mutex> lock(musicControlMutex_);
        activeMusicOwner_ = std::move(slot);
        activeMusic_.store(activeMusicOwner_.get(), std::memory_order_release);
    }
    return true;
}

bool SourceDomainRuntime::startMusic()
{
    if (!auxPrepared_ || !aux_.start()) return false;
    if (cinemaPrepared_.load(std::memory_order_acquire) && !cinema_.start())
        cinemaPrepared_.store(false, std::memory_order_release);

    std::lock_guard<std::mutex> lock(musicControlMutex_);
    if (activeMusicOwner_ && !activeMusicOwner_->stream.start())
    {
        activeMusic_.store(nullptr, std::memory_order_release);
        activeMusicOwner_.reset();
    }
    return true;
}

bool SourceDomainRuntime::clearMusic(std::chrono::milliseconds readerGrace,
                                     std::string& error)
{
    error.clear();
    std::unique_ptr<MusicSlot> old;

    {
        std::lock_guard<std::mutex> lock(musicControlMutex_);
        activeMusic_.store(nullptr, std::memory_order_release);
        old = std::move(activeMusicOwner_);
    }

    if (!old) return true;

    if (!waitForMusicReaders(readerGrace))
    {
        std::lock_guard<std::mutex> lock(musicControlMutex_);
        retiredMusic_.push_back(std::move(old));
        error = "MUSIC reader grace timeout; old runtime retained safely";
        return false;
    }

    old->stream.stop();
    old.reset();
    return true;
}

bool SourceDomainRuntime::activateMusic(std::unique_ptr<SourceModuleResolver> runtime,
                                        std::uint32_t engine,
                                        std::uint64_t sessionRevision,
                                        std::string& error)
{
    error.clear();
    if (!runtime || !runtime->readyStopped() || sessionRevision == 0)
    {
        error = "MUSIC runtime is not a valid stopped candidate";
        return false;
    }

    reapRetiredMusic();

    std::lock_guard<std::mutex> lock(musicControlMutex_);
    if (activeMusicOwner_ ||
        activeMusic_.load(std::memory_order_acquire) != nullptr ||
        !retiredMusic_.empty())
    {
        error = "previous MUSIC runtime is not fully retired";
        return false;
    }

    auto slot = std::make_unique<MusicSlot>();
    slot->ownedRuntime = std::move(runtime);
    slot->runtime = slot->ownedRuntime.get();
    slot->engine = engine;
    slot->sessionRevision = sessionRevision;

    if (!prepareOne(slot->stream, *slot->runtime,
                    "endpoint.tidal", "src.tidal", engine))
    {
        error = "MUSIC SourceDomain prepare failed";
        return false;
    }
    if (!slot->stream.start())
    {
        error = "MUSIC SourceDomain start failed";
        return false;
    }

    activeMusicOwner_ = std::move(slot);
    activeMusic_.store(activeMusicOwner_.get(), std::memory_order_release);
    return true;
}

void SourceDomainRuntime::reapRetiredMusic() noexcept
{
    if (musicReaders_.load(std::memory_order_acquire) != 0)
        return;

    std::lock_guard<std::mutex> lock(musicControlMutex_);
    if (musicReaders_.load(std::memory_order_acquire) != 0)
        return;

    for (auto& slot : retiredMusic_)
        if (slot) slot->stream.stop();
    retiredMusic_.clear();
}

void SourceDomainRuntime::stop() noexcept
{
    std::string ignored;
    clearMusic(std::chrono::milliseconds(500), ignored);

    // Shutdown safety over liveness: never destroy a retireable MUSIC slot
    // while an audio-thread reader still references it.
    while (musicReaders_.load(std::memory_order_acquire) != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    reapRetiredMusic();

    aux_.stop();
    cinemaPrepared_.store(false);
    while (!waitForCinemaReaders(std::chrono::milliseconds(500))) {}
    cinema_.stop();
    auxPrepared_ = false;
    cinemaPrepared_ = false;
    auxRuntime_ = nullptr;
    cinemaRuntime_ = nullptr;
}

bool SourceDomainRuntime::restartMusic(SourceModuleResolver* musicRuntime,
                                       std::uint32_t engine)
{
    // Legacy helper: live dynamic MUSIC switching must use clearMusic() plus
    // activateMusic() from control-plane. Do not block the audio thread here.
    if (activeMusic_.load(std::memory_order_acquire) != nullptr)
        return false;
    if (musicRuntime == nullptr)
        return true;

    auto slot = std::make_unique<MusicSlot>();
    slot->runtime = musicRuntime;
    slot->engine = engine;
    if (!prepareOne(slot->stream, *slot->runtime,
                    "endpoint.tidal", "src.tidal", engine) ||
        !slot->stream.start())
        return false;

    std::lock_guard<std::mutex> lock(musicControlMutex_);
    if (activeMusicOwner_) return false;
    activeMusicOwner_ = std::move(slot);
    activeMusic_.store(activeMusicOwner_.get(), std::memory_order_release);
    return true;
}

bool SourceDomainRuntime::restartAux(std::uint32_t engine)
{
    aux_.stop();
    auxPrepared_ = false;
    auxEngine_ = engine;
    if (!auxRuntime_) return false;

    auxPrepared_ = prepareOne(
        aux_, *auxRuntime_, "endpoint.aux", "src.aux", engine);
    if (!auxPrepared_) return false;
    if (!aux_.start())
    {
        auxPrepared_ = false;
        return false;
    }
    return true;
}

bool SourceDomainRuntime::restartCinema(std::uint32_t engine)
{
    const bool wasPrepared = cinemaPrepared_.exchange(false);
    if (!waitForCinemaReaders(std::chrono::milliseconds(1000)))
    {
        // No storage/state was changed; leave the old engine available safely.
        cinemaPrepared_.store(wasPrepared);
        return false;
    }
    cinema_.stop();
    cinemaEngine_ = engine;
    if (!cinemaRuntime_) return false;

    auto* endpoint = cinemaRuntime_->findInstance("endpoint.cinema");
    auto* src = cinemaRuntime_->findInstance("src.cinema");
    if (!endpoint || !src) return false;
    SourceDomainConfig config;
    config.channels = 8;
    config.destinationRate = 192000;
    config.dspBlockFrames = 1024;
    config.fifoCapacityFrames = RealtimeBufferPolicy::sourceCapacityFrames;
    config.prerollFrames = RealtimeBufferPolicy::sourcePrerollFrames;
    config.captureFifoCapacityFrames =
        RealtimeBufferPolicy::sourceCaptureCapacityFrames;
    config.outputTargetFrames = RealtimeBufferPolicy::sourceTargetFrames;
    config.endpointReadFrames = 1024;
    config.srcEngine = engine;
    if (!cinema_.prepare(*endpoint, *src, config)) return false;
    if (!cinema_.start())
    {
        cinemaPrepared_ = false;
        return false;
    }
    cinemaPrepared_.store(true);
    return true;
}

bool SourceDomainRuntime::popMusic1024(SourceBlock1024& block) noexcept
{
    auto* slot = acquireMusic();
    if (!slot)
    {
        releaseMusic();
        return false;
    }

    std::array<double*, 2> pointers{
        musicBlock_[0].data(), musicBlock_[1].data()};
    SourceStreamDescriptor descriptor{};
    const bool ok = slot->stream.pop1024(pointers.data(), descriptor);
    const auto sessionRevision = slot->sessionRevision;
    releaseMusic();
    if (!ok) return false;

    block = {};
    block.channels = 2;
    block.generation = descriptor.generation;
    block.descriptor = descriptor;
    block.sessionRevision = sessionRevision;
    block.channel[0] = musicBlock_[0].data();
    block.channel[1] = musicBlock_[1].data();
    return true;
}

bool SourceDomainRuntime::popAux1024(SourceBlock1024& block) noexcept
{
    if (!auxPrepared_) return false;

    std::array<double*, 2> pointers{
        auxBlock_[0].data(), auxBlock_[1].data()};
    SourceStreamDescriptor descriptor{};
    if (!aux_.pop1024(pointers.data(), descriptor)) return false;

    block = {};
    block.channels = 2;
    block.generation = descriptor.generation;
    block.descriptor = descriptor;
    block.channel[0] = auxBlock_[0].data();
    block.channel[1] = auxBlock_[1].data();
    return true;
}

bool SourceDomainRuntime::popCinema1024(SourceBlock1024& block) noexcept
{
    if (!acquireCinema()) return false;

    std::array<double*, 8> pointers{};
    for (std::size_t channel = 0; channel < pointers.size(); ++channel)
        pointers[channel] = cinemaBlock_[channel].data();
    SourceStreamDescriptor descriptor{};
    const bool ok = cinema_.pop1024(pointers.data(), descriptor);
    releaseCinema();
    if (!ok) return false;

    block = {};
    block.channels = 8;
    block.generation = descriptor.generation;
    block.descriptor = descriptor;
    for (std::size_t channel = 0; channel < pointers.size(); ++channel)
        block.channel[channel] = cinemaBlock_[channel].data();
    return true;
}

bool SourceDomainRuntime::acquireCinema() const noexcept
{
    // Sequential consistency orders gate closure with reader entry. Readers
    // that observe a closed gate never access CINEMA storage or configuration.
    cinemaReaders_.fetch_add(1);
    if (cinemaPrepared_.load()) return true;
    releaseCinema();
    return false;
}

void SourceDomainRuntime::releaseCinema() const noexcept
{
    cinemaReaders_.fetch_sub(1);
}

bool SourceDomainRuntime::waitForCinemaReaders(
    std::chrono::milliseconds timeout) const noexcept
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (cinemaReaders_.load() != 0)
    {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

SourceDomainTelemetry SourceDomainRuntime::cinemaTelemetry() const noexcept
{
    if (!acquireCinema()) return {};
    const auto result = cinema_.telemetry();
    releaseCinema();
    return result;
}

bool SourceDomainRuntime::cinemaReady() const noexcept
{
    if (!acquireCinema()) return false;
    const bool result = cinema_.ready();
    releaseCinema();
    return result;
}

bool SourceDomainRuntime::requestCinemaSrcEngine(std::uint32_t engine) noexcept
{
    if (!acquireCinema()) return false;
    const bool result = cinema_.requestSrcEngine(engine);
    releaseCinema();
    return result;
}

SourceDomainTelemetry SourceDomainRuntime::musicTelemetry() const noexcept
{
    auto* slot = acquireMusic();
    if (!slot)
    {
        releaseMusic();
        return {};
    }
    const auto telemetry = slot->stream.telemetry();
    releaseMusic();
    return telemetry;
}

bool SourceDomainRuntime::musicReady() const noexcept
{
    auto* slot = acquireMusic();
    if (!slot)
    {
        releaseMusic();
        return false;
    }
    const bool ready = slot->stream.ready();
    releaseMusic();
    return ready;
}

std::uint64_t SourceDomainRuntime::musicSessionRevision() const noexcept
{
    auto* slot = acquireMusic();
    if (!slot)
    {
        releaseMusic();
        return 0;
    }
    const auto revision = slot->sessionRevision;
    releaseMusic();
    return revision;
}

} // namespace entropy::host


