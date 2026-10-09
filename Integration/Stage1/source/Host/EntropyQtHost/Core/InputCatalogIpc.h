#pragma once

#include "InputSourceCatalog.h"

#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host {

inline constexpr wchar_t inputCatalogMappingName[] =
    L"Local\\EntropyAudio.ModularHost.InputCatalog.v1";
inline constexpr wchar_t inputCatalogRefreshMappingName[] =
    L"Local\\EntropyAudio.ModularHost.InputCatalog.Refresh.v1";
inline constexpr std::uint32_t inputCatalogIpcMagic = 0x31434945u; // EIC1
inline constexpr std::uint32_t inputCatalogIpcVersion = 0x00010000u;
inline constexpr std::uint32_t inputCatalogIpcMaximumBytes = 16u * 1024u * 1024u;

struct InputCatalogIpcEntry {
    InputSourceKey key;
    std::string name;
    std::string resourceKey;
    std::string renderEndpointId;
    std::string reason;
    std::uint32_t presence = 0;
    std::uint32_t direction = 0;
    std::uint32_t validation = 0;
    std::uint64_t declaredAccessMask = 0;
    std::uint64_t capabilities = 0;
    bool selectable = false;
    bool legacyUnbound = false;
};

struct InputCatalogIpcSnapshot {
    std::uint64_t revision = 0;
    bool complete = false;
    std::vector<InputCatalogIpcEntry> entries;
    std::vector<std::string> diagnostics;
};

bool publishInputCatalogIpc(const InputSourceSnapshot& snapshot,
                            std::string& error) noexcept;
bool readInputCatalogIpc(InputCatalogIpcSnapshot& snapshot,
                         std::string& error) noexcept;

bool writeInputCatalogRefreshRequest(std::uint64_t& sequence,
                                     std::string& error) noexcept;
bool readInputCatalogRefreshRequest(std::uint64_t& sequence,
                                    std::string& error) noexcept;

} // namespace entropy::host
