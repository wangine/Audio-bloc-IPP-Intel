#include "SourceGraphRuntime.h"
#include <algorithm>
#include <cstring>
#include <entropy/cinema_endpoint_abi.h>
#include <entropy/endpoint_abi.h>
#include <entropy/endpoint_catalog_abi.h>
#include <entropy/input_matrix_abi.h>
#include <entropy/src_abi.h>
namespace entropy::host
{
ModuleInstance *SourceGraphRuntime::findInstance(const char *id) const noexcept
{
    for (size_t i = 0; i < names_.size(); ++i)
        if (names_[i] == id)
            return instances_[i].get();
    return nullptr;
}
bool SourceGraphRuntime::add(const std::filesystem::path &root, const char *file,
                             const char *identifier, const char *id, uint32_t channels,
                             uint32_t rate, std::string &error)
{
    auto library = ModuleLibrary::load(root / file, error);
    if (!library)
        return false;
    const auto &d = library->descriptor();
    if (std::strcmp(d.identifier, identifier) || !(d.module_flags & ENTROPY_MODULE_FLAG_FP64))
    {
        error = "Source descriptor identity/FP64 mismatch";
        return false;
    }
    auto instance = std::make_unique<ModuleInstance>(library);
    if (!instance->create(error))
        return false;
    auto kind = d.module_kind;
    if (kind == ENTROPY_MODULE_ENDPOINT)
    {
        uint32_t frames = 1024;
        if (std::strcmp(identifier, "audio.entropy.endpoint.cinema") == 0)
        {
            EntropyCinemaEndpointApiV1 api{};
            api.struct_size = sizeof(api);
            if (!instance->queryExtension(ENTROPY_CINEMA_ENDPOINT_EXTENSION_ID,
                                          ENTROPY_CINEMA_ENDPOINT_EXTENSION_VERSION, &api,
                                          sizeof(api), error) ||
                !api.connect || !api.disconnect || !api.read_source_f64 || !api.get_status)
                return false;
            frames = (std::max)(frames, api.minimum_buffer_frames);
        }
        else
        {
            EntropyEndpointApiV1 api{};
            api.struct_size = sizeof(api);
            if (!instance->queryExtension(ENTROPY_ENDPOINT_EXTENSION_ID,
                                          ENTROPY_ENDPOINT_EXTENSION_VERSION, &api, sizeof(api),
                                          error) ||
                !api.connect || !api.disconnect || !api.read_source_f64 || !api.get_status)
                return false;
            frames = (std::max)(frames, api.minimum_buffer_frames);
        }
        if (!instance->prepare(192000, frames, 0, channels, error))
            return false;
    }
    else if (!instance->prepare(192000, 1024, channels, channels, error))
        return false;
    // CINEMA compatibility handle stays unconfigured; its worker processes
    // exactly one selected engine (WDL or the explicit r8brain reference).
    if (kind == ENTROPY_MODULE_SRC && std::strcmp(id, "src.cinema") != 0)
    {
        EntropySrcApiV1 api{};
        api.struct_size = sizeof(api);
        if (!instance->queryExtension(ENTROPY_SRC_EXTENSION_ID, ENTROPY_SRC_EXTENSION_VERSION, &api,
                                      sizeof(api), error) ||
            !api.configure || !api.process || !api.get_engine_count || !api.get_engine_descriptor ||
            !api.get_config)
            return false;
        EntropySrcConfig c{};
        c.struct_size = sizeof(c);
        c.engine = ENTROPY_SRC_R8BRAIN_REFERENCE;
        c.source_rate = rate;
        c.destination_rate = 192000;
        c.channels = channels;
        c.maximum_input_frames = 256;
        if (api.configure(instance->handle(), &c) != ENTROPY_OK)
        {
            error = "R10 SRC configure failed";
            return false;
        }
    }
    if (kind == ENTROPY_MODULE_INPUT_MATRIX)
    {
        EntropyInputMatrixApiV1 api{};
        api.struct_size = sizeof(api);
        if (!instance->queryExtension(ENTROPY_INPUT_MATRIX_EXTENSION_ID,
                                      ENTROPY_INPUT_MATRIX_EXTENSION_VERSION, &api, sizeof(api),
                                      error) ||
            !api.set_state)
            return false;
        EntropyInputMatrixState state{};
        state.struct_size = sizeof(state);
        state.mute_tidal = state.mute_aux = 1;
        if (api.set_state(instance->handle(), &state) != ENTROPY_OK)
            return false;
    }
    if (!instance->reset(error))
        return false;
    names_.emplace_back(id);
    instances_.push_back(std::move(instance));
    return true;
}
bool SourceGraphRuntime::prepare(const std::filesystem::path &root, std::string &error)
{
    ready_ = false;
    instances_.clear();
    names_.clear();
    if (!add(root, "EntropyAuxEndpoint.dll", "audio.entropy.endpoint.aux", "endpoint.aux", 2, 0,
             error) ||
        !add(root, "EntropyCinemaInput.dll", "audio.entropy.endpoint.cinema", "endpoint.cinema", 8,
             0, error) ||
        !add(root, "EntropySampleRateConverter.dll", "audio.entropy.src.multiengine.fp64",
             "src.aux", 2, 48000, error) ||
        !add(root, "EntropySampleRateConverter.dll", "audio.entropy.src.multiengine.fp64",
             "src.cinema", 8, 48000, error) ||
        !add(root, "EntropyInputMatrix.dll", "audio.entropy.input-matrix.fp64", "matrix.input", 12,
             0, error))
        return false;
    ready_ = true;
    return true;
}
bool SourceGraphRuntime::prepareMusic(const std::filesystem::path &root, const InputSourceKey &key,
                                      uint64_t revision, uint32_t left, uint32_t right,
                                      std::string &error)
{
    ready_ = false;
    instances_.clear();
    names_.clear();
    const char *file = nullptr;
    if (key.providerId == "audio.entropy.endpoint.music-transport")
        file = "EntropyMusicTransportEndpoint.dll";
    else if (key.providerId == "audio.entropy.endpoint.tidal")
        file = "EntropyTidalEndpoint.dll";
    if (!file || key.endpointId.empty() || !key.accessMode || !revision || left == right)
    {
        error = "MUSIC exact identity incomplete/unsupported";
        return false;
    }
    if (!add(root, file, key.providerId.c_str(), "endpoint.tidal", 2, 0, error) ||
        !add(root, "EntropySampleRateConverter.dll", "audio.entropy.src.multiengine.fp64",
             "src.tidal", 2, 44100, error))
        return false;
    auto *endpoint = findInstance("endpoint.tidal");
    EntropyEndpointCatalogApiV1 api{};
    api.struct_size = sizeof(api);
    if (!endpoint->queryExtension(ENTROPY_ENDPOINT_CATALOG_ID, ENTROPY_ENDPOINT_CATALOG_VERSION,
                                  &api, sizeof(api), error) ||
        !api.bind_stopped)
        return false;
    EntropyEndpointBindRequestV1 request{};
    request.struct_size = sizeof(request);
    request.access_mode = key.accessMode;
    request.revision = revision;
    request.endpoint_id_utf8 = key.endpointId.c_str();
    request.endpoint_id_bytes = static_cast<uint32_t>(key.endpointId.size() + 1);
    request.left_channel = left;
    request.right_channel = right;
    if (api.bind_stopped(endpoint->handle(), &request) != ENTROPY_OK)
    {
        error = "Exact MUSIC bind unavailable/rejected";
        return false;
    }
    ready_ = true;
    return true;
}
} // namespace entropy::host
