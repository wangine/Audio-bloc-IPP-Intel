#include "InputSourceCatalog.h"
#include "ModuleRegistry.h"
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace entropy::host {
namespace {
constexpr std::size_t textBudget = 8u * 1024u * 1024u;
constexpr std::size_t entryBudget = 64u * 1024u * 1024u;
constexpr std::uint64_t supportedMask =
    (UINT64_C(1) << ENTROPY_ACCESS_PRIVATE_TAP) |
    (UINT64_C(1) << ENTROPY_ACCESS_MUSIC_TRANSPORT);
struct BudgetError : std::runtime_error { using std::runtime_error::runtime_error; };
struct Budget {
    std::size_t text = 0, entries = 0;
    void chargeText(std::size_t size) {
        if (size > textBudget - text) throw BudgetError("snapshot text budget exceeded");
        text += size;
    }
    void chargeEntry() {
        if (entries >= entryBudget / sizeof(InputSourceEntry))
            throw BudgetError("snapshot entry-storage budget exceeded");
        ++entries;
    }
};
template<std::size_t N> std::string fixedText(const char (&value)[N]) {
    const auto end = static_cast<const char*>(std::memchr(value, 0, N));
    if (!end) throw std::runtime_error("unterminated module descriptor");
    return std::string(value, end);
}
void check(EntropyResult result, const char* operation) {
    if (result != ENTROPY_OK)
        throw std::runtime_error(std::string(operation) + " result=" + std::to_string(result));
}
bool endpointValid(const EntropyEndpointApiV1& v) {
    return v.struct_size == sizeof(v) && v.extension_version == ENTROPY_ENDPOINT_EXTENSION_VERSION &&
        v.connect && v.disconnect && v.read_source_f64 && v.get_status;
}
bool catalogValid(const EntropyEndpointCatalogApiV1& v) {
    return v.struct_size == sizeof(v) && v.extension_version == ENTROPY_ENDPOINT_CATALOG_VERSION &&
        v.begin_catalog && v.get_device && v.get_text && v.end_catalog && v.bind_stopped && v.cancel_read &&
        std::all_of(std::begin(v.reserved), std::end(v.reserved), [](auto n) { return n == 0; });
}
// Declared after the instance: end_catalog always runs BEFORE instance destruction.
struct FrozenCatalog {
    const EntropyEndpointCatalogApiV1& api;
    EntropyModuleHandle handle;
    std::uint64_t token;
    ~FrozenCatalog() { api.end_catalog(handle, token); }
};
std::string readText(const EntropyEndpointCatalogApiV1& api, EntropyModuleHandle handle,
    std::uint64_t snapshot, std::uint64_t index, std::uint32_t field, Budget& budget) {
    std::uint32_t needed = 0;
    check(api.get_text(handle, snapshot, index, field, nullptr, 0, &needed), "get_text size");
    if (needed == 0) throw std::runtime_error("get_text omitted NUL byte size");
    budget.chargeText(needed); // validate before allocation; no truncation/retry loop
    std::vector<char> bytes(needed, '\x7f');
    std::uint32_t confirmed = 0;
    check(api.get_text(handle, snapshot, index, field, bytes.data(), needed, &confirmed), "get_text copy");
    if (confirmed != needed || bytes.back() != '\0' ||
        std::find(bytes.begin(), bytes.end() - 1, '\0') != bytes.end() - 1)
        throw std::runtime_error("get_text size/NUL changed inside frozen snapshot");
    return std::string(bytes.data(), needed - 1u);
}
bool directionAllows(std::uint32_t mode, std::uint32_t direction) {
    switch (mode) {
        case ENTROPY_ACCESS_PRIVATE_TAP: return direction == ENTROPY_CATALOG_DIRECTION_PRIVATE_TAP;
        case ENTROPY_ACCESS_MUSIC_TRANSPORT:
            return direction == ENTROPY_CATALOG_DIRECTION_MUSIC_TRANSPORT;
        default: return false;
    }
}

bool productionMusicAccess(std::uint32_t mode) noexcept {
    switch (mode) {
        case ENTROPY_ACCESS_PRIVATE_TAP:
        case ENTROPY_ACCESS_MUSIC_TRANSPORT:
            return true;
        default:
            return false;
    }
}
std::vector<InputSourceEntry> enumerate(const std::shared_ptr<ModuleLibrary>& library,
    const std::string& providerId, const EntropyEndpointCatalogApiV1& api, Budget& budget) {
    ModuleInstance instance(library);
    std::string error;
    if (!instance.create(error)) throw std::runtime_error(error);
    EntropyEndpointCatalogInfoV1 info{}; info.struct_size = sizeof(info);
    check(api.begin_catalog(instance.handle(), &info), "begin_catalog");
    FrozenCatalog frozen{api, instance.handle(), info.snapshot};
    if (info.struct_size != sizeof(info) || info.reserved_u32 != 0)
        throw std::runtime_error("invalid catalog snapshot header");
    if (info.count > entryBudget / sizeof(InputSourceEntry))
        throw BudgetError("snapshot entry-storage budget exceeded");
    std::vector<InputSourceEntry> pending;
    for (std::uint64_t index = 0; index < info.count; ++index) {
        EntropyEndpointDeviceInfoV1 device{}; device.struct_size = sizeof(device);
        check(api.get_device(instance.handle(), info.snapshot, index, &device), "get_device");
        if (device.struct_size != sizeof(device)) throw std::runtime_error("invalid device structure size");
        std::array<std::string, 5> text;
        for (std::uint32_t field = 1; field <= 5; ++field)
            text[field - 1] = readText(api, instance.handle(), info.snapshot, index, field, budget);
        if (text[0].empty()) throw std::runtime_error("missing exact endpoint identity");
        // One row per known access mode. Unknown modes stay visible but unselectable.
        std::vector<std::uint32_t> modes;
        for (const auto mode : { ENTROPY_ACCESS_PRIVATE_TAP,
                                 ENTROPY_ACCESS_MUSIC_TRANSPORT })
            if ((device.access_mask & (UINT64_C(1) << mode)) != 0) modes.push_back(mode);
        if (modes.empty() || (device.access_mask & ~supportedMask) != 0) modes.push_back(ENTROPY_ACCESS_NONE);
        bool first = true;
        for (const auto mode : modes) {
            budget.chargeEntry(); budget.chargeText(providerId.size() + 1);
            if (!first) for (const auto& s : text) budget.chargeText(s.size() + 1);
            first = false;
            InputSourceEntry row;
            row.key = {providerId, text[0], mode}; row.name = text[1]; row.renderEndpointId = text[2];
            row.resourceKey = text[3]; row.reason = text[4]; row.presence = device.presence;
            row.direction = device.direction; row.validation = device.validation;
            row.declaredAccessMask = device.access_mask; row.capabilities = device.capabilities;
            const bool directionValid = directionAllows(mode, device.direction);
            const bool productionAccess = productionMusicAccess(mode);
            row.selectable = device.presence == ENTROPY_CATALOG_PRESENT &&
                             directionValid && productionAccess;
            if (directionValid && !productionAccess) {
                if (!row.reason.empty()) row.reason += "; ";
                row.reason += "developer/diagnostic only; not a production MUSIC owner";
            }
            pending.push_back(std::move(row));
        }
    }
    return pending;
}
} // namespace

InputSourceCatalog::InputSourceCatalog() : controlThread_(std::this_thread::get_id()) {}
InputSourceSnapshot InputSourceCatalog::refresh(const std::filesystem::path& providerRoot) {
    if (std::this_thread::get_id() != controlThread_)
        throw std::logic_error("InputSourceCatalog refresh requires its owning control thread");
    if (revision_ == (std::numeric_limits<std::uint64_t>::max)())
        throw std::overflow_error("catalog revision exhausted");
    InputSourceSnapshot snapshot; snapshot.revision = ++revision_;
    Budget budget;
    try {
        ModuleRegistry registry;
        registry.scan(std::filesystem::absolute(providerRoot));
        std::unordered_map<std::string, std::size_t> identifiers;
        for (const auto& record : registry.records())
            if (record.state == ModuleLoadState::loaded && record.descriptor.module_kind == ENTROPY_MODULE_ENDPOINT)
                ++identifiers[fixedText(record.descriptor.identifier)];
        for (const auto& record : registry.records()) {
            if (record.state != ModuleLoadState::loaded) {
                snapshot.complete = false;
                snapshot.diagnostics.push_back("rejected module: " + record.error); continue;
            }
            if (record.descriptor.module_kind != ENTROPY_MODULE_ENDPOINT) continue;
            const auto providerId = fixedText(record.descriptor.identifier);
            if (identifiers[providerId] != 1) {
                snapshot.complete = false;
                snapshot.diagnostics.push_back("duplicate provider identifier: " + providerId); continue;
            }
            try {
                std::string error;
                auto library = ModuleLibrary::load(record.path, error);
                if (!library) throw std::runtime_error(error);
                if (fixedText(library->descriptor().identifier) != providerId ||
                    library->descriptor().module_kind != ENTROPY_MODULE_ENDPOINT)
                    throw std::runtime_error("provider identity changed after registry inspection");
                snapshot.libraries.push_back(library);
                const auto query = library->api().query_extension;
                EntropyEndpointApiV1 endpoint{}; endpoint.struct_size = sizeof(endpoint);
                if (!query || query(ENTROPY_ENDPOINT_EXTENSION_ID, ENTROPY_ENDPOINT_EXTENSION_VERSION,
                        &endpoint, sizeof(endpoint)) != ENTROPY_OK || !endpointValid(endpoint))
                    throw std::runtime_error("incompatible Endpoint V1");
                EntropyEndpointCatalogApiV1 catalog{}; catalog.struct_size = sizeof(catalog);
                const auto result = query(ENTROPY_ENDPOINT_CATALOG_ID, ENTROPY_ENDPOINT_CATALOG_VERSION,
                    &catalog, sizeof(catalog));
                if (result == ENTROPY_ERROR_UNSUPPORTED) {
                    InputSourceEntry row; row.key.providerId = providerId;
                    row.name = fixedText(library->descriptor().name); row.legacyUnbound = true;
                    row.reason = "legacyUnbound: catalog extension unavailable";
                    budget.chargeEntry(); budget.chargeText(providerId.size() + row.name.size() + row.reason.size() + 3);
                    snapshot.entries.push_back(std::move(row));
                    snapshot.diagnostics.push_back(providerId + ": legacyUnbound (no bind/connect/read attempted)");
                    continue; // no create/prepare/reset/connect probes for legacy
                }
                if (result != ENTROPY_OK || !catalogValid(catalog)) throw std::runtime_error("invalid catalog ABI");
                auto pending = enumerate(library, providerId, catalog, budget);
                snapshot.entries.insert(snapshot.entries.end(), std::make_move_iterator(pending.begin()),
                    std::make_move_iterator(pending.end()));
            } catch (const BudgetError&) { throw; }
            catch (const std::exception& error) {
                snapshot.complete = false;
                snapshot.diagnostics.push_back(providerId + ": enumeration failed: " + error.what());
            }
        }
    } catch (const std::exception& error) {
        // Aggregate/resource failure: never publish a deceptively truncated list.
        snapshot.complete = false; snapshot.entries.clear(); snapshot.libraries.clear();
        snapshot.diagnostics.push_back(std::string("catalog rejected: ") + error.what());
    }
    return snapshot;
}
InputSourceFindResult findInputSource(const InputSourceSnapshot& snapshot,
                                          const InputSourceKey& key) noexcept {
    const InputSourceEntry* exact = nullptr;
    std::size_t exactCount = 0;
    bool endpointIdentitySeen = false;

    for (const auto& entry : snapshot.entries) {
        if (entry.key.providerId != key.providerId ||
            entry.key.endpointId != key.endpointId)
            continue;

        endpointIdentitySeen = true;
        if (entry.key.accessMode != key.accessMode)
            continue;

        exact = &entry;
        ++exactCount;
        if (exactCount > 1)
            return {InputSourceFindStatus::ambiguous, nullptr};
    }

    if (exactCount == 1)
        return {InputSourceFindStatus::found, exact};
    if (endpointIdentitySeen)
        return {InputSourceFindStatus::incompatible, nullptr};
    return {InputSourceFindStatus::absent, nullptr};
}
} // namespace entropy::host
