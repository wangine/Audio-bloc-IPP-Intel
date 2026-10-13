#include "DspRecoveryModule.h"
#include "DspNativeRegistry.h"
#include <cmath>
#include <cstring>

namespace entropy::stage {
EntropyDynamicRecoveryConfigV1 defaultRecoverySettings() noexcept {
    EntropyDynamicRecoveryConfigV1 c{};
    c.struct_size = sizeof(c);
    c.version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    c.transient_strength = 0.35;
    c.onset_threshold_db = 2;
    c.max_attack_boost_db = 2;
    c.expansion_ratio = 1.15;
    c.expansion_threshold_dbfs = -18;
    c.max_expansion_boost_db = 1.5;
    c.max_total_boost_db = 3;
    c.wet_mix = 1;
    return c;
}

bool validRecoverySettings(const EntropyDynamicRecoveryConfigV1& c) noexcept {
    const auto bounded = [](double v, double lo, double hi) {
        return std::isfinite(v) && v >= lo && v <= hi;
    };
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
    for (uint64_t value : c.reserved) if (value) return false;
    return true;
}

void DspRecoveryModule::close() noexcept {
    if (handle_ && api_.destroy) api_.destroy(handle_);
    handle_ = nullptr;
    if (library_) FreeLibrary(library_);
    library_ = nullptr;
    api_ = {};
    snapshot_ = {};
}

bool DspRecoveryModule::prepare(const std::filesystem::path& root,
    const EntropyDynamicRecoveryConfigV1& config, std::string& error) {
    close();
    if (!validRecoverySettings(config)) { error = "Invalid Dynamic Recovery settings"; return false; }
    library_ = loadNativeModule(NativeModuleId::DynamicRecovery, root, error);
    if (!library_) return false;
    auto get = reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library_, ENTROPY_MODULE_GET_API_SYMBOL));
    api_.struct_size = sizeof(api_);
    EntropyModuleDescriptor d{};
    d.struct_size = sizeof(d);
    if (!get || get(ENTROPY_MODULE_ABI_VERSION, &api_) != ENTROPY_OK ||
        !api_.get_descriptor || !api_.create || !api_.destroy || !api_.prepare ||
        !api_.reset || !api_.set_config || !api_.process_f64 || !api_.latency_samples ||
        api_.get_descriptor(&d) != ENTROPY_OK || d.max_input_channels != 2 ||
        d.max_output_channels != 2 || !(d.module_flags & ENTROPY_MODULE_FLAG_FP64) ||
        std::strcmp(d.identifier, "audio.entropy.dynamic-recovery.fp64.v1")) {
        error = "Dynamic Recovery FP64/2IN-2OUT ABI rejected";
        close();
        return false;
    }
    EntropyHostServices host{};
    host.struct_size = sizeof(host);
    host.abi_version = ENTROPY_MODULE_ABI_VERSION;
    EntropyPrepareInfo info{};
    info.struct_size = sizeof(info);
    info.sample_rate = 192000;
    info.maximum_frames = 1024;
    info.input_channels = info.output_channels = 2;
    if (api_.create(&host, &handle_) != ENTROPY_OK || !handle_ ||
        api_.prepare(handle_, &info) != ENTROPY_OK || !apply(config)) {
        error = "Dynamic Recovery native preparation failed";
        close();
        return false;
    }
    uint32_t latency = 1;
    if (api_.latency_samples(handle_, &latency) != ENTROPY_OK || latency != 0) {
        error = "Dynamic Recovery latency must be zero";
        close();
        return false;
    }
    snapshot_.loaded = 1;
    snapshot_.latencySamples = latency;
    return true;
}

bool DspRecoveryModule::apply(const EntropyDynamicRecoveryConfigV1& config,
    uint64_t revision) noexcept {
    if (!handle_ || !validRecoverySettings(config)) return false;
    EntropyConfigBlob blob{};
    blob.struct_size = sizeof(blob);
    blob.schema_id = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_ID;
    blob.schema_version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    blob.byte_count = sizeof(config);
    blob.bytes = &config;
    if (api_.set_config(handle_, &blob) != ENTROPY_OK) return false;
    snapshot_.settings = config;
    snapshot_.active = config.enabled && config.wet_mix > 0 &&
        ((config.transient_strength > 0 && config.max_attack_boost_db > 0) ||
         (config.expansion_enabled && config.max_expansion_boost_db > 0));
    snapshot_.revision = revision;
    return true;
}

bool DspRecoveryModule::process(const double* left, const double* right,
    double* outLeft, double* outRight) noexcept {
    if (!handle_ || !active()) return false;
    const double* inputs[]{left, right};
    double* outputs[]{outLeft, outRight};
    EntropyAudioBlockF64 block{};
    block.struct_size = sizeof(block);
    block.input_channels = block.output_channels = 2;
    block.frames = 1024;
    block.inputs = inputs;
    block.outputs = outputs;
    EntropyProcessContext context{};
    context.struct_size = sizeof(context);
    return api_.process_f64(handle_, &block, &context) == ENTROPY_OK;
}

bool DspRecoveryModule::reset() noexcept {
    return handle_ && api_.reset(handle_) == ENTROPY_OK;
}
}
