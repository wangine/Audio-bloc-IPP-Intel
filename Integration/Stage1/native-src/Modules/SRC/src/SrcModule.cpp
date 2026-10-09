#include "MultiChannelSrcEngine.h"

#include <entropy/module_abi.h>
#include <entropy/src_abi.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace
{
using entropy::src::MultiChannelSrcEngine;

struct SrcState { MultiChannelSrcEngine engine; EntropyPrepareInfo prepare {}; bool prepared = false; };
struct EngineDefinition { std::uint32_t engine; const char* identifier; const char* name; };
constexpr std::array<EngineDefinition, 1> engines {{
    { ENTROPY_SRC_R8BRAIN_REFERENCE, "r8brain-reference", "r8brain-free-src Reference" }
}};

void copyText(char* destination, std::size_t capacity, const char* source) noexcept
{
    if (capacity == 0) return;
    const auto* text = source != nullptr ? source : "";
    const auto length = (std::min)(std::strlen(text), capacity - 1);
    std::memcpy(destination, text, length);
    destination[length] = '\0';
}
SrcState* stateFrom(EntropyModuleHandle handle) noexcept { return reinterpret_cast<SrcState*>(handle); }

EntropyResult ENTROPY_ABI_CALL getDescriptor(EntropyModuleDescriptor* descriptor)
{
    if (descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *descriptor = {};
    descriptor->struct_size = sizeof(*descriptor);
    descriptor->abi_version = ENTROPY_MODULE_ABI_VERSION;
    descriptor->module_kind = ENTROPY_MODULE_SRC;
    descriptor->module_flags = ENTROPY_MODULE_FLAG_REALTIME_SAFE_PROCESS |
                               ENTROPY_MODULE_FLAG_FP64 |
                               ENTROPY_MODULE_FLAG_REPORTS_LATENCY;
    descriptor->version_major = 2; descriptor->version_minor = 1; descriptor->version_patch = 9;
    descriptor->max_input_channels = ENTROPY_SRC_MAX_CHANNELS;
    descriptor->max_output_channels = ENTROPY_SRC_MAX_CHANNELS;
    copyText(descriptor->identifier, sizeof(descriptor->identifier), "audio.entropy.src.multiengine.fp64");
    copyText(descriptor->name, sizeof(descriptor->name), "Entropy Sample Rate Converter FP64");
    copyText(descriptor->vendor, sizeof(descriptor->vendor), "Entropy Audio");
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL createModule(const EntropyHostServices* host, EntropyModuleHandle* result)
{
    if (host == nullptr || result == nullptr || host->struct_size < sizeof(*host) || host->abi_version != ENTROPY_MODULE_ABI_VERSION) return ENTROPY_ERROR_INVALID_ARGUMENT;
    try { *result = reinterpret_cast<EntropyModuleHandle>(new SrcState {}); return ENTROPY_OK; }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}
void ENTROPY_ABI_CALL destroyModule(EntropyModuleHandle handle) { delete stateFrom(handle); }

EntropyResult ENTROPY_ABI_CALL prepareModule(EntropyModuleHandle handle, const EntropyPrepareInfo* info)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || info == nullptr || info->struct_size < sizeof(*info) || info->sample_rate == 0 || info->maximum_frames == 0 || info->input_channels < 1 || info->input_channels > ENTROPY_SRC_MAX_CHANNELS || info->output_channels != info->input_channels) return ENTROPY_ERROR_INVALID_ARGUMENT;
    state->prepare = *info; state->prepared = true; return ENTROPY_OK;
}
EntropyResult ENTROPY_ABI_CALL resetModule(EntropyModuleHandle handle)
{
    auto* state = stateFrom(handle);
    if (state == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (!state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    state->engine.reset(); return ENTROPY_OK;
}
EntropyResult ENTROPY_ABI_CALL setConfig(EntropyModuleHandle, const EntropyConfigBlob*) { return ENTROPY_ERROR_UNSUPPORTED; }
EntropyResult ENTROPY_ABI_CALL processCommon(EntropyModuleHandle, const EntropyAudioBlockF64*, const EntropyProcessContext*) { return ENTROPY_ERROR_UNSUPPORTED; }
EntropyResult ENTROPY_ABI_CALL latencyCommon(EntropyModuleHandle handle, std::uint32_t* samples)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || samples == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (!state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    *samples = state->engine.latencyOutputFrames(); return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getEngineCount(std::uint32_t* count)
{ if (count == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT; *count = static_cast<std::uint32_t>(engines.size()); return ENTROPY_OK; }
EntropyResult ENTROPY_ABI_CALL getEngineDescriptor(std::uint32_t index, EntropySrcEngineDescriptor* descriptor)
{
    if (index >= engines.size() || descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *descriptor = {}; descriptor->struct_size = sizeof(*descriptor); descriptor->engine = engines[index].engine;
    copyText(descriptor->identifier, sizeof(descriptor->identifier), engines[index].identifier);
    copyText(descriptor->name, sizeof(descriptor->name), engines[index].name); return ENTROPY_OK;
}
EntropyResult ENTROPY_ABI_CALL configure(EntropyModuleHandle handle, EntropySrcConfig* config)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || config == nullptr || config->struct_size < sizeof(*config)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (!state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    if (config->channels != state->prepare.input_channels || config->maximum_input_frames > state->prepare.maximum_frames) return ENTROPY_ERROR_INVALID_ARGUMENT;
    try { if (!state->engine.configure(*config)) return ENTROPY_ERROR_INVALID_ARGUMENT; *config = state->engine.config(); return ENTROPY_OK; }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}
EntropyResult ENTROPY_ABI_CALL getConfig(EntropyModuleHandle handle, EntropySrcConfig* config)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || config == nullptr || config->struct_size < sizeof(*config)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (state->engine.config().source_rate == 0) return ENTROPY_ERROR_NOT_PREPARED;
    *config = state->engine.config(); return ENTROPY_OK;
}
EntropyResult ENTROPY_ABI_CALL processSrc(EntropyModuleHandle handle, EntropySrcBlockF64* block)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || block == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (!state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    return state->engine.process(*block);
}
EntropyResult ENTROPY_ABI_CALL queryExtension(const char* identifier, std::uint32_t version, void* extensionApi, std::uint32_t extensionApiSize)
{
    if (identifier == nullptr || extensionApi == nullptr || std::strcmp(identifier, ENTROPY_SRC_EXTENSION_ID) != 0) return ENTROPY_ERROR_UNSUPPORTED;
    if (version != ENTROPY_SRC_EXTENSION_VERSION || extensionApiSize < sizeof(EntropySrcApiV1)) return ENTROPY_ERROR_ABI_MISMATCH;
    auto* api = static_cast<EntropySrcApiV1*>(extensionApi); *api = {};
    api->struct_size = sizeof(*api); api->extension_version = ENTROPY_SRC_EXTENSION_VERSION;
    api->get_engine_count = getEngineCount; api->get_engine_descriptor = getEngineDescriptor;
    api->configure = configure; api->get_config = getConfig; api->process = processSrc;
    api->latency_output_frames = latencyCommon; return ENTROPY_OK;
}
}

extern "C" ENTROPY_ABI_EXPORT EntropyResult ENTROPY_ABI_CALL entropy_module_get_api(std::uint32_t requestedAbiVersion, EntropyModuleApiV1* api)
{
    if (api == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (requestedAbiVersion != ENTROPY_MODULE_ABI_VERSION || api->struct_size < sizeof(*api)) return ENTROPY_ERROR_ABI_MISMATCH;
    *api = {}; api->struct_size = sizeof(*api); api->abi_version = ENTROPY_MODULE_ABI_VERSION;
    api->get_descriptor = getDescriptor; api->create = createModule; api->destroy = destroyModule;
    api->prepare = prepareModule; api->reset = resetModule; api->set_config = setConfig;
    api->process_f64 = processCommon; api->latency_samples = latencyCommon; api->query_extension = queryExtension;
    return ENTROPY_OK;
}
