#pragma once

#include "InputSourceCatalog.h"

#include <cstdint>
#include <string>

namespace entropy::host {

inline constexpr wchar_t musicSelectionRequestMappingName[] =
    L"Local\\EntropyAudio.ModularHost.MusicSelection.Request.v1";
inline constexpr wchar_t musicSelectionStatusMappingName[] =
    L"Local\\EntropyAudio.ModularHost.MusicSelection.Status.v1";
inline constexpr std::uint32_t musicSelectionIpcMagic = 0x31534d45u; // EMS1
inline constexpr std::uint32_t musicSelectionIpcVersion = 0x00010000u;
inline constexpr std::uint32_t musicSelectionIpcMaximumBytes = 1024u * 1024u;

enum class MusicSelectionState : std::uint32_t {
    unbound = 0,
    requested = 1,
    boundStopped = 2,
    unavailable = 3,
    fault = 4,
    connecting = 5,
    active = 6
};

struct MusicSourceSelectionRequest {
    std::uint64_t revision = 0;
    InputSourceKey key;
    std::uint32_t leftChannel = 0;
    std::uint32_t rightChannel = 1;
    std::string label = "MUSIC";
};

struct MusicSourceSelectionStatus {
    std::uint64_t requestedRevision = 0;
    std::uint64_t appliedRevision = 0;
    MusicSelectionState state = MusicSelectionState::unbound;
    InputSourceKey requestedKey;
    InputSourceKey appliedKey;
    std::uint32_t leftChannel = 0;
    std::uint32_t rightChannel = 1;
    std::string label = "MUSIC";
    std::string detail = "UNBOUND";
};

bool writeMusicSourceSelectionRequest(
    const InputSourceKey& key,
    std::uint32_t leftChannel,
    std::uint32_t rightChannel,
    const std::string& label,
    std::uint64_t& revision,
    std::string& error) noexcept;

bool readMusicSourceSelectionRequest(
    MusicSourceSelectionRequest& request,
    std::string& error) noexcept;

bool publishMusicSourceSelectionStatus(
    const MusicSourceSelectionStatus& status,
    std::string& error) noexcept;

bool readMusicSourceSelectionStatus(
    MusicSourceSelectionStatus& status,
    std::string& error) noexcept;

} // namespace entropy::host
