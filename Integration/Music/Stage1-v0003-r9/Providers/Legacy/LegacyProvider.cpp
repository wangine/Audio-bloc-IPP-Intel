#include "endpoint_provider_v2.h"
#include "endpoint_stream_state_v1.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <entropy/module_abi.h>
#include <filesystem>
#include <windows.h>
namespace {
struct Module {
    HMODULE library{};
    EntropyModuleApiV1 api{};
    EntropyEndpointApiV1 endpoint{};
    EntropyEndpointCatalogApiV1 catalog{};
    Module() {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&enumerate), &self);
        wchar_t path[32768]{};
        GetModuleFileNameW(self, path, 32768);
        auto root = std::filesystem::path(path).parent_path() / L"reference" / LEGACY_FILE;
        library = LoadLibraryExW(root.c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!library)
            return;
        auto entry =
            reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library, ENTROPY_MODULE_GET_API_SYMBOL));
        api.struct_size = sizeof(api);
        if (!entry || entry(ENTROPY_MODULE_ABI_VERSION, &api) != ENTROPY_OK || !api.query_extension) {
            api = {};
            return;
        }
        endpoint.struct_size = sizeof(endpoint);
        catalog.struct_size = sizeof(catalog);
        if (api.query_extension(ENTROPY_ENDPOINT_EXTENSION_ID, ENTROPY_ENDPOINT_EXTENSION_VERSION, &endpoint,
                                sizeof(endpoint)) != ENTROPY_OK) {
            endpoint = {};
            return;
        }
#if !defined(LEGACY_FIXED_AUX)
        if (
            api.query_extension(ENTROPY_ENDPOINT_CATALOG_ID, ENTROPY_ENDPOINT_CATALOG_VERSION, &catalog,
                                sizeof(catalog)) != ENTROPY_OK ||
            !catalog.cancel_read) {
            endpoint = {};
            catalog = {};
        }
#endif
    }
    ~Module() {
        if (library)
            FreeLibrary(library);
    }
    static int32_t enumerate(uint32_t, EntropyEndpointInfoV2 *);
};
Module &module() {
    static Module m;
    return m;
}
int32_t Module::enumerate(uint32_t index, EntropyEndpointInfoV2 *out) {
    auto &m = module();
#if defined(LEGACY_FIXED_AUX)
    if (index || !out || !m.api.create || !m.api.prepare || !m.endpoint.connect || !m.endpoint.read_source_f64)
        return 0;
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->roles = ENTROPY_ROLE_AUX;
    out->eligible = 1;
    out->accessModes = 1u << LEGACY_ACCESS;
    strcpy_s(out->endpointId, "entropy-aux:system-default");
    strcpy_s(out->name, "Entropy AUX · Windows default audio · original private bridge");
    return 1;
#else
    if (!m.api.create || !m.catalog.begin_catalog)
        return 0;
    EntropyHostServices services{};
    services.struct_size = sizeof(services);
    services.abi_version = ENTROPY_MODULE_ABI_VERSION;
    EntropyModuleHandle handle = nullptr;
    if (m.api.create(&services, &handle) != ENTROPY_OK)
        return -1;
    EntropyEndpointCatalogInfoV1 snapshot{};
    snapshot.struct_size = sizeof(snapshot);
    int result = 0;
    if (m.catalog.begin_catalog(handle, &snapshot) == ENTROPY_OK) {
        if (index < snapshot.count) {
            EntropyEndpointDeviceInfoV1 device{};
            device.struct_size = sizeof(device);
            *out = {};
            out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
            out->structSize = sizeof(*out);
            out->roles = LEGACY_ROLES;
            out->state = 0;
            out->accessModes = uint32_t(1u << LEGACY_ACCESS);
            uint32_t required = 0;
            if (m.catalog.get_device(handle, snapshot.snapshot, index, &device) == ENTROPY_OK &&
                m.catalog.get_text(handle, snapshot.snapshot, index, ENTROPY_CATALOG_TEXT_ENDPOINT_ID,
                                   out->endpointId, sizeof(out->endpointId), &required) == ENTROPY_OK &&
                m.catalog.get_text(handle, snapshot.snapshot, index, ENTROPY_CATALOG_TEXT_FRIENDLY_NAME,
                                   out->name, sizeof(out->name), &required) == ENTROPY_OK) {
                m.catalog.get_text(handle, snapshot.snapshot, index, ENTROPY_CATALOG_TEXT_RENDER_ID,
                                   out->windowsEndpointId, sizeof(out->windowsEndpointId), &required);
                out->eligible = device.presence == ENTROPY_CATALOG_PRESENT &&
                                (device.access_mask & (UINT64_C(1) << LEGACY_ACCESS));
                out->state = device.presence;
                result = 1;
            }
        }
        m.catalog.end_catalog(handle, snapshot.snapshot);
    }
    m.api.destroy(handle);
    return result;
#endif
}
struct Stream {
    EntropyModuleHandle handle{};
    uint64_t lastGeneration{}, lastLoss{};
    std::atomic<bool> canceled{false};
    ~Stream() {
        auto &m = module();
        if (handle) {
            m.endpoint.disconnect(handle);
            m.api.destroy(handle);
        }
    }
};
int32_t open(const char *id, uint32_t mode, void **out) {
    auto &m = module();
    if (mode != LEGACY_ACCESS || !id || !out)
        return -1;
#if defined(LEGACY_FIXED_AUX)
    if (strcmp(id, "entropy-aux:system-default") || !m.endpoint.connect || !m.api.create || !m.api.prepare)
        return -1;
#else
    if (!m.catalog.bind_stopped) return -1;
#endif
    auto stream = new Stream;
    EntropyHostServices services{};
    services.struct_size = sizeof(services);
    services.abi_version = ENTROPY_MODULE_ABI_VERSION;
    if (m.api.create(&services, &stream->handle) != ENTROPY_OK) {
        delete stream;
        return -1;
    }
    EntropyEndpointBindRequestV1 binding{};
    binding.struct_size = sizeof(binding);
    binding.access_mode = mode;
    binding.revision = 1;
    binding.endpoint_id_utf8 = id;
    binding.endpoint_id_bytes = static_cast<uint32_t>(strlen(id) + 1);
    binding.right_channel = 1;
    EntropyPrepareInfo prepare{};
    prepare.struct_size = sizeof(prepare);
    prepare.sample_rate = 192000;
    prepare.maximum_frames = (std::max)(1024u, m.endpoint.minimum_buffer_frames);
    prepare.output_channels = 2;
    if (
#if !defined(LEGACY_FIXED_AUX)
        m.catalog.bind_stopped(stream->handle, &binding) != ENTROPY_OK ||
#endif
        m.api.prepare(stream->handle, &prepare) != ENTROPY_OK ||
        m.endpoint.connect(stream->handle) != ENTROPY_OK) {
        delete stream;
        return -1;
    }
    *out = stream;
    return 1;
}
int32_t read(void *opaque, EntropyIngressPacketV2 *out, void *payload, uint32_t bytes) {
    auto &s = *static_cast<Stream *>(opaque);
    auto &m = module();
    if (s.canceled) return -1;
    uint32_t capacity = bytes / 16;
    if (capacity < m.endpoint.minimum_buffer_frames)
        return -1;
    EntropyEndpointBlockF64 block{};
    block.struct_size = sizeof(block);
    block.capacity_frames = capacity;
    block.left = static_cast<double *>(payload);
    block.right = block.left + capacity;
    if (m.endpoint.read_source_f64(s.handle, &block) != ENTROPY_OK)
        return -1;
    if (!block.frame_count) {
#if defined(LEGACY_FIXED_AUX)
        *out = {};
        out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
        out->structSize = sizeof(*out);
        out->generation = block.status.stream_generation;
        out->flags = ENTROPY_PACKET_STREAM_STATE_V1;
        out->reserved[0] = block.status.state == ENTROPY_ENDPOINT_ACTIVE
                            ? ENTROPY_STREAM_RUNNING_V1 : ENTROPY_STREAM_IDLE_V1;
#endif
        return 0;
    }
    if (block.frame_count > capacity || block.status.channels != 2 || !block.status.stream_generation)
        return -1;
    std::memmove(block.left + block.frame_count, block.right, block.frame_count * sizeof(double));
    *out = {};
    out->abiVersion = ENTROPY_PROVIDER_ABI_V2;
    out->structSize = sizeof(*out);
    out->generation = block.status.stream_generation;
    // V1 lacks upstream sequence/firstFrame and raw subtype; never invent these.
    out->originalContainerBits = block.status.container_bits;
    out->originalValidBits = block.status.valid_bits;
    out->frames = block.frame_count;
    out->payloadBytes = block.frame_count * 16;
    out->mode = ENTROPY_MODE_UNKNOWN;
#if defined(LEGACY_FIXED_AUX)
    const auto truth = block.status.discontinuity_flags &
        (ENTROPY_ENDPOINT_FLAG_SHARED_MIX_RATE | ENTROPY_ENDPOINT_FLAG_EXCLUSIVE_NATIVE_RATE | ENTROPY_ENDPOINT_FLAG_MODE_UNKNOWN);
    if (truth == ENTROPY_ENDPOINT_FLAG_SHARED_MIX_RATE) out->mode = ENTROPY_MODE_SHARED;
    if (truth == ENTROPY_ENDPOINT_FLAG_EXCLUSIVE_NATIVE_RATE) out->mode = ENTROPY_MODE_EXCLUSIVE;
#endif
    out->flags = ENTROPY_PACKET_METADATA_PARTIAL;
    if (s.lastGeneration != out->generation || s.lastLoss != block.status.lost_frames)
        out->flags |= ENTROPY_PACKET_DISCONTINUITY;
    s.lastGeneration = out->generation;
    s.lastLoss = block.status.lost_frames;
    out->lostFrames = s.lastLoss;
    LARGE_INTEGER q, f;
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&f);
    out->qpcTimestamp = q.QuadPart;
    out->qpcFrequency = f.QuadPart;
    auto &fmt = out->format;
    fmt.structSize = sizeof(fmt);
    fmt.sampleRate = block.status.source_rate;
    fmt.channels = 2;
    fmt.containerBits = fmt.validBits = 64;
    fmt.blockAlign = 16;
    fmt.averageBytesPerSecond = fmt.sampleRate * 16;
    fmt.channelMask = 3;
    fmt.representation = ENTROPY_REP_F64_PLANAR;
    return 1;
}
void cancel(void *h) {
    auto& s=*static_cast<Stream*>(h);s.canceled=true;
    if (module().catalog.cancel_read) module().catalog.cancel_read(s.handle);
    // Original AUX V1 read is a nonblocking bridge query. Do not disconnect
    // its handle concurrently; the ingress worker joins before close().
}
void close(void *h) { delete static_cast<Stream *>(h); }
} // namespace
extern "C" ENTROPY_PROVIDER_EXPORT int32_t entropy_endpoint_provider_v2(uint32_t abi,
                                                                        EntropyProviderApiV2 *out) {
    if (abi != ENTROPY_PROVIDER_ABI_V2 || out->structSize != sizeof(*out))
        return -1;
    *out = {};
    out->abiVersion = abi;
    out->structSize = sizeof(*out);
    out->info.abiVersion = abi;
    out->info.structSize = sizeof(out->info);
    out->info.roles = LEGACY_ROLES;
    out->info.capabilities = ENTROPY_CAP_CONVERTED_F64;
#if defined(LEGACY_FIXED_AUX)
    out->info.capabilities |= ENTROPY_CAP_STREAM_STATE_V1;
#endif
    strcpy_s(out->info.providerId, LEGACY_ID);
    strcpy_s(out->info.name, LEGACY_NAME);
    out->enumerate = Module::enumerate;
    out->open = open;
    out->read = read;
    out->cancel = cancel;
    out->close = close;
    return 1;
}
