#pragma once
#include "InputSourceCatalog.h"
#include <entropy/endpoint_abi.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace entropy::host {

enum class MusicPacketResult : std::uint32_t {
    accepted = 0,
    empty = 1,
    fault = 2
};

struct MusicSourcePacket {
    std::uint64_t sessionRevision = 0;
    std::uint64_t streamGeneration = 0;
    std::uint32_t sourceRate = 0;
    std::uint32_t containerBits = 0;
    std::uint32_t validBits = 0;
    std::uint32_t discontinuityFlags = 0;
    std::uint32_t frameCount = 0;
    std::vector<double> left;
    std::vector<double> right;
};

MusicPacketResult buildMusicSourcePacket(const EntropyEndpointBlockF64& block,
                                         std::uint64_t sessionRevision,
                                         MusicSourcePacket& packet,
                                         std::uint64_t& receivedFrames,
                                         std::string& error);

bool packetMatchesSession(const MusicSourcePacket& packet,
                          std::uint64_t expectedSessionRevision) noexcept;

enum class MusicBindingState : std::uint32_t {
    unbound = 0,
    binding = 1,
    bound = 2,
    connecting = 3,
    active = 4,
    unavailable = 5,
    fault = 6
};

enum class MusicBindingResult : std::uint32_t {
    pending = 0,
    rejectedRunning = 1,
    unavailable = 2,
    resourceConflict = 3,
    exclusivityUnknown = 4,
    timeout = 5,
    fault = 6
};

struct MusicBindingStatus {
    MusicBindingState state = MusicBindingState::unbound;
    std::uint64_t requestedRevision = 0;
    std::uint64_t appliedRevision = 0;
    bool muted = true;
    bool readerActive = false;
    bool resourceExclusivityUnknown = false;
    std::string detail;
};

class MusicResourceRegistry final {
public:
    bool reserveExternal(std::string resourceKey, std::string owner);
    bool isOwnedByOther(std::string_view resourceKey, std::string_view owner) const;
    bool claim(std::string resourceKey, std::string owner);
    void releaseOwner(std::string_view owner) noexcept;

private:
    struct Lease {
        std::string key;
        std::string owner;
    };
    std::vector<Lease> leases_;
};

class MusicSourceBinding final {
public:
    explicit MusicSourceBinding(MusicResourceRegistry& resources,
                                std::string owner = "MUSIC");
    ~MusicSourceBinding();

    MusicSourceBinding(const MusicSourceBinding&) = delete;
    MusicSourceBinding& operator=(const MusicSourceBinding&) = delete;

    MusicBindingResult requestSwitch(const InputSourceSnapshot& snapshot,
                                     const InputSourceKey& key,
                                     std::uint64_t revision,
                                     bool musicMutedOrStopped,
                                     std::chrono::milliseconds joinTimeout =
                                         std::chrono::milliseconds(100));

    bool confirmConnected(std::uint64_t revision);
    bool shutdown(std::chrono::milliseconds joinTimeout =
                      std::chrono::milliseconds(100));

    const MusicBindingStatus& status() const noexcept { return status_; }
    const std::vector<std::string>& lifecycleLog() const noexcept {
        return lifecycleLog_;
    }

private:
    struct Runtime;

    bool stopReader(std::chrono::milliseconds timeout);
    void destroyCurrent() noexcept;

    MusicResourceRegistry& resources_;
    std::string owner_;
    MusicBindingStatus status_{};
    std::vector<std::string> lifecycleLog_;
    std::unique_ptr<Runtime> runtime_;
};

} // namespace entropy::host
