#pragma once
#include "endpoint_provider_v2.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
namespace entropy::music {
struct LoadedProvider {
    HMODULE library{};
    EntropyProviderApiV2 api{};
    ~LoadedProvider() {
        if (library)
            FreeLibrary(library);
    }
};
struct ProviderEndpoint {
    LoadedProvider *provider{};
    EntropyEndpointInfoV2 endpoint{};
};
struct SourceKey {
    std::string providerId, endpointId;
    bool operator==(const SourceKey &) const = default;
};
class ProviderRegistry {
    std::vector<std::unique_ptr<LoadedProvider>> providers_;

  public:
    std::vector<std::string> errors;
    bool discover(const std::filesystem::path &);
    std::vector<ProviderEndpoint> enumerate();
    LoadedProvider *find(const std::string &) const noexcept;
};
class SourceSelector {
    ProviderRegistry &registry_;
    SourceKey keys_[2];

  public:
    explicit SourceSelector(ProviderRegistry &r) : registry_(r) {}
    bool select(uint32_t role, const SourceKey &);
    const SourceKey &selected(uint32_t role) const { return keys_[role == ENTROPY_ROLE_AUX ? 1 : 0]; }
};
} // namespace entropy::music
