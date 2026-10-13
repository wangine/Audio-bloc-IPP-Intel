#include <entropy/module_abi.h>
#include <entropy/dynamic_recovery_abi.h>
#include "FaustGlue.h"
#define FAUSTFLOAT double
#include "../Generated/EntropyDynamicRecoveryFaust.cpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

namespace {
EntropyDynamicRecoveryConfigV1 defaults() noexcept {
    EntropyDynamicRecoveryConfigV1 c{};
    c.struct_size = sizeof(c);
    c.version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    c.transient_strength = 0.35;
    c.onset_threshold_db = 2.0;
    c.max_attack_boost_db = 2.0;
    c.expansion_ratio = 1.15;
    c.expansion_threshold_dbfs = -18.0;
    c.max_expansion_boost_db = 1.5;
    c.max_total_boost_db = 3.0;
    c.wet_mix = 1.0;
    return c;
}

struct Zones final : UI {
    double* transient_strength = nullptr;
    double* onset_threshold_db = nullptr;
    double* max_attack_boost_db = nullptr;
    double* expansion_enabled = nullptr;
    double* expansion_ratio = nullptr;
    double* expansion_threshold_dbfs = nullptr;
    double* max_expansion_boost_db = nullptr;
    double* max_total_boost_db = nullptr;
    double* wet_mix = nullptr;
    void openVerticalBox(const char*) override {}
    void closeBox() override {}
    void addHorizontalSlider(const char* label, double* zone,
        double, double, double, double) override {
        struct Entry { const char* name; double** destination; };
        const Entry entries[]{
            {"transient_strength", &transient_strength},
            {"onset_threshold_db", &onset_threshold_db},
            {"max_attack_boost_db", &max_attack_boost_db},
            {"expansion_enabled", &expansion_enabled},
            {"expansion_ratio", &expansion_ratio},
            {"expansion_threshold_dbfs", &expansion_threshold_dbfs},
            {"max_expansion_boost_db", &max_expansion_boost_db},
            {"max_total_boost_db", &max_total_boost_db},
            {"wet_mix", &wet_mix}};
        for (const auto& entry : entries)
            if (std::strcmp(label, entry.name) == 0) { *entry.destination = zone; return; }
    }
    bool complete() const noexcept {
        return transient_strength && onset_threshold_db && max_attack_boost_db &&
            expansion_enabled && expansion_ratio && expansion_threshold_dbfs &&
            max_expansion_boost_db && max_total_boost_db && wet_mix;
    }
    void apply(const EntropyDynamicRecoveryConfigV1& c) noexcept {
        *transient_strength = c.transient_strength;
        *onset_threshold_db = c.onset_threshold_db;
        *max_attack_boost_db = c.max_attack_boost_db;
        *expansion_enabled = static_cast<double>(c.expansion_enabled);
        *expansion_ratio = c.expansion_ratio;
        *expansion_threshold_dbfs = c.expansion_threshold_dbfs;
        *max_expansion_boost_db = c.max_expansion_boost_db;
        *max_total_boost_db = c.max_total_boost_db;
        *wet_mix = c.wet_mix;
    }
};

bool bounded(double value, double low, double high) noexcept {
    return std::isfinite(value) && value >= low && value <= high;
}

bool valid(const EntropyDynamicRecoveryConfigV1& c) noexcept {
    if (c.struct_size != sizeof(c) || c.version != ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION ||
        c.enabled > 1 || c.expansion_enabled > 1 ||
        !bounded(c.transient_strength, 0, 1) ||
        !bounded(c.onset_threshold_db, 0, 12) ||
        !bounded(c.max_attack_boost_db, 0, 6) ||
        !bounded(c.expansion_ratio, 1, 1.5) ||
        !bounded(c.expansion_threshold_dbfs, -36, -6) ||
        !bounded(c.max_expansion_boost_db, 0, 3) ||
        !bounded(c.max_total_boost_db, 0, 6) ||
        !bounded(c.wet_mix, 0, 1)) return false;
    for (uint64_t word : c.reserved) if (word) return false;
    return true;
}

bool overlaps(const void* a, const void* b, size_t bytes) noexcept {
    const auto startA = reinterpret_cast<uintptr_t>(a);
    const auto startB = reinterpret_cast<uintptr_t>(b);
    return bytes && startA < startB + bytes && startB < startA + bytes;
}
} // namespace

struct EntropyModuleOpaque {
    EntropyDynamicRecoveryFaust dsp;
    Zones zones;
    EntropyDynamicRecoveryConfigV1 config = defaults();
    uint32_t maximum_frames = 0;
    bool prepared = false;
};

namespace {
EntropyResult ENTROPY_ABI_CALL get_descriptor(EntropyModuleDescriptor* out) {
    if (!out || out->struct_size < sizeof(*out)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    EntropyModuleDescriptor d{};
    d.struct_size = sizeof(d);
    d.abi_version = ENTROPY_MODULE_ABI_VERSION;
    d.module_kind = ENTROPY_MODULE_UNKNOWN;
    d.module_flags = ENTROPY_MODULE_FLAG_REALTIME_SAFE_PROCESS |
        ENTROPY_MODULE_FLAG_FP64 | ENTROPY_MODULE_FLAG_REPORTS_LATENCY;
    d.version_major = 0;
    d.version_minor = 1;
    d.version_patch = 0;
    d.max_input_channels = d.max_output_channels = 2;
    std::strcpy(d.identifier, "audio.entropy.dynamic-recovery.fp64.v1");
    std::strcpy(d.name, "Entropy Dynamic Recovery");
    std::strcpy(d.vendor, "Entropy Audio");
    *out = d;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL create(const EntropyHostServices* host, EntropyModuleHandle* out) {
    if (!host || host->struct_size < sizeof(*host) ||
        host->abi_version != ENTROPY_MODULE_ABI_VERSION || !out)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *out = new (std::nothrow) EntropyModuleOpaque;
    if (*out) {
        (*out)->dsp.buildUserInterface(&(*out)->zones);
        if (!(*out)->zones.complete()) { delete *out; *out = nullptr; }
    }
    return *out ? ENTROPY_OK : ENTROPY_ERROR_INVALID_STATE;
}

void ENTROPY_ABI_CALL destroy(EntropyModuleHandle handle) { delete handle; }

EntropyResult ENTROPY_ABI_CALL prepare(EntropyModuleHandle handle, const EntropyPrepareInfo* info) {
    if (!handle || !info || info->struct_size < sizeof(*info) ||
        info->input_channels != 2 || info->output_channels != 2 ||
        info->sample_rate < 8000 || info->sample_rate > 192000 ||
        info->maximum_frames == 0 || info->maximum_frames > 65536)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    handle->dsp.init(static_cast<int>(info->sample_rate));
    handle->zones.apply(handle->config);
    handle->maximum_frames = info->maximum_frames;
    handle->prepared = true;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL reset(EntropyModuleHandle handle) {
    if (!handle || !handle->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    handle->dsp.instanceClear();
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL set_config(EntropyModuleHandle handle, const EntropyConfigBlob* blob) {
    if (!handle || !blob || blob->struct_size < sizeof(*blob) ||
        blob->schema_id != ENTROPY_DYNAMIC_RECOVERY_SCHEMA_ID ||
        blob->schema_version != ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION ||
        blob->byte_count != sizeof(EntropyDynamicRecoveryConfigV1) || !blob->bytes)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    EntropyDynamicRecoveryConfigV1 candidate{};
    std::memcpy(&candidate, blob->bytes, sizeof(candidate));
    if (!valid(candidate)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    handle->config = candidate;
    if (handle->prepared) handle->zones.apply(candidate);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL process(EntropyModuleHandle handle,
    const EntropyAudioBlockF64* block, const EntropyProcessContext* context) {
    if (!handle || !handle->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    if (!block || block->struct_size < sizeof(*block) ||
        block->input_channels != 2 || block->output_channels != 2 ||
        block->frames > handle->maximum_frames || !block->inputs || !block->outputs ||
        !block->inputs[0] || !block->inputs[1] || !block->outputs[0] || !block->outputs[1] ||
        (context && context->struct_size < sizeof(*context)))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const size_t bytes = static_cast<size_t>(block->frames) * sizeof(double);
    if (overlaps(block->outputs[0], block->outputs[1], bytes) ||
        overlaps(block->outputs[0], block->inputs[1], bytes) ||
        overlaps(block->outputs[1], block->inputs[0], bytes))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto& c = handle->config;
    if (c.enabled && c.wet_mix > 0 &&
        ((c.transient_strength > 0 && c.max_attack_boost_db > 0) ||
         (c.expansion_enabled && c.max_expansion_boost_db > 0))) {
        if (overlaps(block->inputs[0], block->outputs[0], bytes) ||
            overlaps(block->inputs[1], block->outputs[1], bytes))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        for (uint32_t i = 0; i < block->frames; ++i)
            if (!std::isfinite(block->inputs[0][i]) || !std::isfinite(block->inputs[1][i]))
                return ENTROPY_ERROR_PROCESSING;
        if (context && (context->flags & ENTROPY_PROCESS_DISCONTINUITY))
            handle->dsp.instanceClear();
        double* in[]{const_cast<double*>(block->inputs[0]), const_cast<double*>(block->inputs[1])};
        double* out[]{block->outputs[0], block->outputs[1]};
        handle->dsp.compute(static_cast<int>(block->frames), in, out);
        return ENTROPY_OK;
    }
    if (context && (context->flags & ENTROPY_PROCESS_DISCONTINUITY))
        handle->dsp.instanceClear();
    for (unsigned channel = 0; channel < 2; ++channel)
        std::memmove(block->outputs[channel], block->inputs[channel], bytes);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL latency_samples(EntropyModuleHandle handle, uint32_t* out) {
    if (!handle || !out) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *out = 0;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL query_extension(const char*, uint32_t, void*, uint32_t) {
    return ENTROPY_ERROR_UNSUPPORTED;
}
} // namespace

extern "C" ENTROPY_ABI_EXPORT EntropyResult ENTROPY_ABI_CALL entropy_module_get_api(
    uint32_t requested_version, EntropyModuleApiV1* out) {
    if (!out || out->struct_size < sizeof(*out)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (requested_version != ENTROPY_MODULE_ABI_VERSION) return ENTROPY_ERROR_ABI_MISMATCH;
    EntropyModuleApiV1 api{};
    api.struct_size = sizeof(api);
    api.abi_version = ENTROPY_MODULE_ABI_VERSION;
    api.get_descriptor = get_descriptor;
    api.create = create;
    api.destroy = destroy;
    api.prepare = prepare;
    api.reset = reset;
    api.set_config = set_config;
    api.process_f64 = process;
    api.latency_samples = latency_samples;
    api.query_extension = query_extension;
    *out = api;
    return ENTROPY_OK;
}
