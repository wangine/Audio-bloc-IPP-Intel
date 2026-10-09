#pragma once

#include "SourceModuleResolver.h"
#include "CinemaSourceDomainStream.h"
#include "SourceDomainStream.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace entropy::host {

struct SourceBlock1024 {
    SourceStreamDescriptor descriptor{};
    std::uint32_t channels = 0;
    std::uint64_t generation = 0;
    std::uint64_t sessionRevision = 0; // Captured under the same MUSIC slot lease as payload.
    std::array<const double*, 8> channel{};
};

class SourceDomainRuntime final {
public:
    SourceDomainRuntime() = default;
    ~SourceDomainRuntime();

    SourceDomainRuntime(const SourceDomainRuntime&) = delete;
    SourceDomainRuntime& operator=(const SourceDomainRuntime&) = delete;

    static constexpr std::uint32_t engineRate() noexcept { return 192000; }
    static constexpr std::uint32_t blockFrames() noexcept { return 1024; }

    // Startup: AUX is mandatory baseline. MUSIC may be null/unbound.
    bool prepareMusic(SourceModuleResolver* initialMusicRuntime,
                      SourceModuleResolver& auxRuntime,
                      SourceModuleResolver* cinemaRuntime,
                      std::uint32_t musicEngine,
                      std::uint32_t auxEngine,
                      std::uint32_t cinemaEngine);
    bool startMusic();
    void stop() noexcept;

    // Control-plane MUSIC lifecycle. These methods never touch AUX.
    bool clearMusic(std::chrono::milliseconds readerGrace,
                    std::string& error);
    bool activateMusic(std::unique_ptr<SourceModuleResolver> runtime,
                       std::uint32_t engine,
                       std::uint64_t sessionRevision,
                       std::string& error);
    void reapRetiredMusic() noexcept;

    // Legacy/reconfiguration helpers.
    bool restartMusic(SourceModuleResolver* musicRuntime, std::uint32_t engine);
    bool restartAux(std::uint32_t engine);
    bool restartCinema(std::uint32_t engine);
    bool requestCinemaSrcEngine(std::uint32_t engine) noexcept;

    // Audio-thread readers. Returned pointers reference stable buffers owned by
    // SourceDomainRuntime, never by a retireable MUSIC slot.
    bool popMusic1024(SourceBlock1024&) noexcept;
    bool popAux1024(SourceBlock1024&) noexcept;
    bool popCinema1024(SourceBlock1024&) noexcept;

    SourceDomainTelemetry musicTelemetry() const noexcept;
    SourceDomainTelemetry auxTelemetry() const noexcept { return aux_.telemetry(); }
    SourceDomainTelemetry cinemaTelemetry() const noexcept;
    bool musicPrepared() const noexcept {
        return activeMusic_.load(std::memory_order_acquire) != nullptr;
    }
    bool musicReady() const noexcept;
    std::uint64_t musicSessionRevision() const noexcept;
    bool auxPrepared() const noexcept { return auxPrepared_; }
    bool cinemaPrepared() const noexcept {
        return cinemaPrepared_.load(std::memory_order_acquire);
    }
    bool cinemaReady() const noexcept;

    // Historical aliases retained while naming migration completes.
    bool restartTidal(std::uint32_t engine) {
        return restartMusic(nullptr, engine);
    }
    bool popTidal1024(SourceBlock1024& block) noexcept {
        return popMusic1024(block);
    }
    SourceDomainTelemetry tidalTelemetry() const noexcept {
        return musicTelemetry();
    }

private:
    struct MusicSlot
    {
        std::unique_ptr<SourceModuleResolver> ownedRuntime;
        SourceModuleResolver* runtime = nullptr;
        SourceDomainStream stream;
        std::uint32_t engine = 0;
        std::uint64_t sessionRevision = 0;
    };

    bool prepareOne(SourceDomainStream&, SourceModuleResolver&,
                    const char*, const char*, std::uint32_t);
    MusicSlot* acquireMusic() const noexcept;
    void releaseMusic() const noexcept;
    bool waitForMusicReaders(std::chrono::milliseconds timeout) const noexcept;

    // CINEMA control-plane reconfiguration excludes only CINEMA readers.
    bool acquireCinema() const noexcept;
    void releaseCinema() const noexcept;
    bool waitForCinemaReaders(std::chrono::milliseconds timeout) const noexcept;
    mutable std::atomic<std::uint32_t> cinemaReaders_{0};
    mutable std::atomic<std::uint32_t> musicReaders_{0};
    std::atomic<MusicSlot*> activeMusic_{nullptr};

    mutable std::mutex musicControlMutex_;
    std::unique_ptr<MusicSlot> activeMusicOwner_;
    std::vector<std::unique_ptr<MusicSlot>> retiredMusic_;

    SourceModuleResolver* auxRuntime_ = nullptr;
    SourceModuleResolver* cinemaRuntime_ = nullptr;
    SourceDomainStream aux_;
    CinemaSourceDomainStream cinema_;
    std::array<std::array<double, 1024>, 2> musicBlock_{};
    std::array<std::array<double, 1024>, 2> auxBlock_{};
    std::array<std::array<double, 1024>, 8> cinemaBlock_{};
    std::uint32_t auxEngine_ = 0;
    std::uint32_t cinemaEngine_ = 0;
    bool auxPrepared_ = false;
    std::atomic<bool> cinemaPrepared_{false};
};

} // namespace entropy::host


