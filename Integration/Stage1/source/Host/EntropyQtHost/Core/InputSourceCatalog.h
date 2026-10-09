#pragma once
#include "ModuleRuntime.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace entropy::host {
struct InputSourceKey {
    std::string providerId;
    std::string endpointId;
    std::uint32_t accessMode = 0;
    bool operator==(const InputSourceKey&) const = default;
};
struct InputSourceEntry {
    InputSourceKey key;
    std::string name, resourceKey, renderEndpointId, reason;
    std::uint32_t presence = 0, direction = 0, validation = 0;
    std::uint64_t declaredAccessMask = 0, capabilities = 0;
    bool selectable = false, legacyUnbound = false;
};
enum class InputSourceFindStatus : std::uint32_t { found = 0, absent = 1, ambiguous = 2, incompatible = 3 };
struct InputSourceFindResult {
    InputSourceFindStatus status = InputSourceFindStatus::absent;
    const InputSourceEntry* entry = nullptr;
};
struct InputSourceSnapshot {
    std::uint64_t revision = 0;
    bool complete = true;
    std::vector<InputSourceEntry> entries;
    std::vector<std::string> diagnostics;
    // Host/control-plane only. Pins code while any snapshot copy survives.
    // Never serialize or place these C++ objects across ABI/IPC.
    std::vector<std::shared_ptr<ModuleLibrary>> libraries;
};
InputSourceFindResult findInputSource(const InputSourceSnapshot&, const InputSourceKey&) noexcept;

class InputSourceCatalog final {
public:
    InputSourceCatalog(); // Construct and refresh on the SAME control thread.
    InputSourceCatalog(const InputSourceCatalog&) = delete;
    InputSourceCatalog& operator=(const InputSourceCatalog&) = delete;
    InputSourceSnapshot refresh(const std::filesystem::path& providerRoot);
private:
    std::thread::id controlThread_;
    std::uint64_t revision_ = 0;
};
} // namespace entropy::host
