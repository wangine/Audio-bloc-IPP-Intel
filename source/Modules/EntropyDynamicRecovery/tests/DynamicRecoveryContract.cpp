#include <entropy/module_abi.h>
#include <entropy/dynamic_recovery_abi.h>
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

static int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 2) return fail("DLL path required");
    HMODULE library = LoadLibraryA(argv[1]);
    if (!library) return fail("EntropyDynamicRecovery.dll is missing or cannot load");
    auto get = reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library, ENTROPY_MODULE_GET_API_SYMBOL));
    if (!get) return fail("C ABI export missing");
    EntropyModuleApiV1 api{};
    api.struct_size = sizeof(api);
    if (get(ENTROPY_MODULE_ABI_VERSION, &api) != ENTROPY_OK ||
        !api.get_descriptor || !api.create || !api.destroy || !api.prepare ||
        !api.reset || !api.set_config || !api.process_f64 || !api.latency_samples)
        return fail("ABI v1 contract rejected");
    EntropyModuleDescriptor descriptor{};
    descriptor.struct_size = sizeof(descriptor);
    if (api.get_descriptor(&descriptor) != ENTROPY_OK ||
        descriptor.max_input_channels != 2 || descriptor.max_output_channels != 2 ||
        !(descriptor.module_flags & ENTROPY_MODULE_FLAG_FP64))
        return fail("FP64 stereo descriptor rejected");
    EntropyHostServices host{};
    host.struct_size = sizeof(host);
    host.abi_version = ENTROPY_MODULE_ABI_VERSION;
    EntropyModuleHandle handle = nullptr;
    if (api.create(&host, &handle) != ENTROPY_OK || !handle) return fail("create failed");
    EntropyPrepareInfo prepare{};
    prepare.struct_size = sizeof(prepare);
    prepare.sample_rate = 192000;
    prepare.maximum_frames = 1024;
    prepare.input_channels = prepare.output_channels = 2;
    if (api.prepare(handle, &prepare) != ENTROPY_OK) return fail("prepare failed");
    uint32_t latency = 99;
    if (api.latency_samples(handle, &latency) != ENTROPY_OK || latency != 0)
        return fail("zero latency contract failed");
    const std::array<uint64_t, 8> patterns{
        0x0000000000000000ULL, 0x8000000000000000ULL,
        0x0000000000000001ULL, 0x8000000000000001ULL,
        0x3ff0000000000000ULL, 0x4008000000000000ULL,
        0x7ff8000000000042ULL, 0xfff8000000000043ULL};
    std::array<double, 8> left{}, right{}, outLeft{}, outRight{};
    std::memcpy(left.data(), patterns.data(), sizeof(left));
    for (size_t i = 0; i < patterns.size(); ++i) {
        const uint64_t bits = patterns[patterns.size() - i - 1];
        std::memcpy(&right[i], &bits, sizeof(bits));
    }
    const double* inputs[]{left.data(), right.data()};
    double* outputs[]{outLeft.data(), outRight.data()};
    EntropyAudioBlockF64 block{};
    block.struct_size = sizeof(block);
    block.input_channels = block.output_channels = 2;
    block.frames = static_cast<uint32_t>(left.size());
    block.inputs = inputs;
    block.outputs = outputs;
    EntropyProcessContext context{};
    context.struct_size = sizeof(context);
    if (api.process_f64(handle, &block, &context) != ENTROPY_OK ||
        std::memcmp(left.data(), outLeft.data(), sizeof(left)) ||
        std::memcmp(right.data(), outRight.data(), sizeof(right)))
        return fail("default bypass is not bit exact");

    EntropyDynamicRecoveryConfigV1 config{};
    config.struct_size = sizeof(config);
    config.version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    config.enabled = 1;
    config.transient_strength = 0.35;
    config.onset_threshold_db = 2.0;
    config.max_attack_boost_db = 2.0;
    config.expansion_ratio = 1.15;
    config.expansion_threshold_dbfs = -18.0;
    config.max_expansion_boost_db = 1.5;
    config.max_total_boost_db = 3.0;
    config.wet_mix = 1.0;
    EntropyConfigBlob blob{};
    blob.struct_size = sizeof(blob);
    blob.schema_id = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_ID;
    blob.schema_version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    blob.byte_count = sizeof(config);
    blob.bytes = &config;
    if (api.set_config(handle, &blob) != ENTROPY_OK) return fail("active config rejected");

    std::array<double, 1024> signalL{}, signalR{}, processedL{}, processedR{};
    const double* activeIn[]{signalL.data(), signalR.data()};
    double* activeOut[]{processedL.data(), processedR.data()};
    block.frames = 1024;
    block.inputs = activeIn;
    block.outputs = activeOut;
    for (int blockIndex = 0; blockIndex < 20; ++blockIndex) {
        signalL.fill(0.04); signalR.fill(0.02);
        if (api.process_f64(handle, &block, &context) != ENTROPY_OK) return fail("warm-up failed");
    }
    signalL.fill(0.04); signalR.fill(0.02);
    for (int i = 0; i < 64; ++i) { signalL[i] = 0.8; signalR[i] = 0.4; }
    if (api.process_f64(handle, &block, &context) != ENTROPY_OK)
        return fail("active transient process failed");
    const double gainL = processedL[32] / signalL[32];
    const double gainR = processedR[32] / signalR[32];
    if (!(gainL > 1.02) || std::abs(gainL - gainR) > 1.0e-12)
        return fail("transient boost or stereo link failed");
    for (int blockIndex = 0; blockIndex < 30; ++blockIndex) {
        signalL.fill(0.04); signalR.fill(0.02);
        if (api.process_f64(handle, &block, &context) != ENTROPY_OK) return fail("release failed");
    }
    if (std::abs(processedL.back() / signalL.back() - 1.0) > 0.002)
        return fail("stationary gain did not return to unity");

    config.transient_strength = std::nan("");
    if (api.set_config(handle, &blob) != ENTROPY_ERROR_INVALID_ARGUMENT)
        return fail("NaN config accepted");
    config.transient_strength = 0.35;
    config.expansion_ratio = 1.6;
    if (api.set_config(handle, &blob) != ENTROPY_ERROR_INVALID_ARGUMENT)
        return fail("out-of-range config accepted");
    config.expansion_ratio = 1.15;
    block.frames = 1025;
    if (api.process_f64(handle, &block, &context) != ENTROPY_ERROR_INVALID_ARGUMENT)
        return fail("oversized block accepted");
    block.frames = 1024;
    double* aliasOut[]{signalL.data(), signalR.data()};
    block.outputs = aliasOut;
    if (api.process_f64(handle, &block, &context) != ENTROPY_ERROR_INVALID_ARGUMENT)
        return fail("active in-place processing accepted");
    block.outputs = activeOut;
    signalL[0] = std::nan("");
    if (api.process_f64(handle, &block, &context) != ENTROPY_ERROR_PROCESSING)
        return fail("nonfinite active input accepted");
    signalL[0] = 0.04;
    if (api.process_f64(handle, &block, &context) != ENTROPY_OK ||
        !std::isfinite(processedL[0]))
        return fail("nonfinite input contaminated DSP state");

    config.transient_strength = 0.0;
    config.expansion_enabled = 1;
    config.expansion_ratio = 1.5;
    config.expansion_threshold_dbfs = -36.0;
    if (api.set_config(handle, &blob) != ENTROPY_OK) return fail("expansion config rejected");
    context.flags = ENTROPY_PROCESS_DISCONTINUITY;
    signalL.fill(0.2); signalR.fill(0.1);
    for (int blockIndex = 0; blockIndex < 30; ++blockIndex) {
        if (api.process_f64(handle, &block, &context) != ENTROPY_OK)
            return fail("expansion process failed");
        context.flags = ENTROPY_PROCESS_NONE;
    }
    if (!(processedL.back() / signalL.back() > 1.05) ||
        std::abs(processedL.back() / signalL.back() - processedR.back() / signalR.back()) > 1.0e-12)
        return fail("upward expansion or stereo link failed");

    config.wet_mix = 0.0;
    if (api.set_config(handle, &blob) != ENTROPY_OK) return fail("wet zero config rejected");
    block.frames = static_cast<uint32_t>(left.size());
    block.inputs = inputs;
    block.outputs = outputs;
    if (api.process_f64(handle, &block, &context) != ENTROPY_OK ||
        std::memcmp(left.data(), outLeft.data(), sizeof(left)) ||
        std::memcmp(right.data(), outRight.data(), sizeof(right)))
        return fail("wet zero bypass is not bit exact");
    api.destroy(handle);
    FreeLibrary(library);
    return 0;
}
