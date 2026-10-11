#include "ProviderRegistry.h"
#include "endpoint_stream_state_v1.h"
#include <cstring>
namespace entropy::music {
namespace {
template <size_t N> bool terminated(const char (&s)[N]) { return std::memchr(s, 0, N) && s[0]; }
} // namespace
bool ProviderRegistry::discover(const std::filesystem::path &directory) {
    if (!providers_.empty()) {
        errors.emplace_back("Registry already frozen for process lifetime");
        return false;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
        return false;
    for (const auto &file : std::filesystem::directory_iterator(directory)) {
        if (file.path().extension() != L".dll" || !file.is_regular_file())
            continue;
        auto provider = std::make_unique<LoadedProvider>();
        provider->library =
            LoadLibraryExW(std::filesystem::absolute(file.path()).c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        auto fail = [&](const char *why) { errors.push_back(file.path().filename().string() + ": " + why); };
        if (!provider->library) {
            fail("LoadLibrary failed");
            continue;
        }
        auto entry = reinterpret_cast<EntropyProviderEntryV2>(
            GetProcAddress(provider->library, "entropy_endpoint_provider_v2"));
        auto &api = provider->api;
        api.structSize = sizeof(api);
        if (!entry || entry(ENTROPY_PROVIDER_ABI_V2, &api) != 1 ||
            api.abiVersion != ENTROPY_PROVIDER_ABI_V2 || api.structSize != sizeof(api) ||
            api.info.abiVersion != ENTROPY_PROVIDER_ABI_V2 || api.info.structSize != sizeof(api.info) ||
            !terminated(api.info.providerId) || !terminated(api.info.name) || !api.info.roles ||
            (api.info.roles & ~3u) || !api.info.capabilities || (api.info.capabilities & ~(3u | ENTROPY_CAP_STREAM_STATE_V1)) ||
            !api.enumerate || !api.open || !api.read || !api.cancel || !api.close) {
            fail("Incompatible provider ABI");
            continue;
        }
        bool reserved = false;
        for (auto v : api.reserved)
            reserved |= v != 0;
        for (auto v : api.info.reserved)
            reserved |= v != 0;
        if (reserved || find(api.info.providerId)) {
            fail("Reserved fields or duplicate provider identity");
            continue;
        }
        providers_.push_back(std::move(provider));
    }
    return !providers_.empty();
}
std::vector<ProviderEndpoint> ProviderRegistry::enumerate() {
    std::vector<ProviderEndpoint> result;
    for (auto &p : providers_)
        for (uint32_t i = 0; i < 256; i++) {
            EntropyEndpointInfoV2 e{};
            e.abiVersion = ENTROPY_PROVIDER_ABI_V2;
            e.structSize = sizeof(e);
            auto rc = p->api.enumerate(i, &e);
            if (rc != 1)
                break;
            if (e.structSize != sizeof(e) || e.abiVersion != ENTROPY_PROVIDER_ABI_V2 ||
                !terminated(e.endpointId) || !terminated(e.name) ||
                !std::memchr(e.windowsEndpointId, 0, sizeof(e.windowsEndpointId)) || !e.roles ||
                (e.roles & ~p->api.info.roles) || e.eligible > 1 || e.reserved) {
                errors.emplace_back("Invalid endpoint metadata");
                break;
            }
            result.push_back({p.get(), e});
        }
    return result;
}
LoadedProvider *ProviderRegistry::find(const std::string &id) const noexcept {
    for (auto &p : providers_)
        if (id == p->api.info.providerId)
            return p.get();
    return nullptr;
}
bool SourceSelector::select(uint32_t role, const SourceKey &key) {
    if (role != 1 && role != 2)
        return false;
    keys_[role == 2 ? 1 : 0] = key;
    for (const auto &e : registry_.enumerate())
        if (key.providerId == e.provider->api.info.providerId && key.endpointId == e.endpoint.endpointId)
            return e.endpoint.eligible && (e.endpoint.roles & role);
    return false;
}
} // namespace entropy::music
