#include <entropy/module_abi.h>
#include <entropy/src_abi.h>
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
constexpr double pi = 3.141592653589793238462643383279502884;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct LoadedModule
{
    HMODULE library = nullptr;
    EntropyModuleApiV1 common {};
    EntropySrcApiV1 src {};
    EntropyModuleHandle handle = nullptr;

    explicit LoadedModule(const char* path, std::uint32_t channels = 8, std::uint32_t maximumFrames = 1024)
    {
        library = LoadLibraryA(path); require(library != nullptr, "LoadLibrary failed");
        auto getApi = reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library, ENTROPY_MODULE_GET_API_SYMBOL));
        require(getApi != nullptr, "entropy_module_get_api missing");
        common.struct_size = sizeof(common);
        require(getApi(ENTROPY_MODULE_ABI_VERSION, &common) == ENTROPY_OK, "common ABI rejected");
        src.struct_size = sizeof(src);
        require(common.query_extension(ENTROPY_SRC_EXTENSION_ID, ENTROPY_SRC_EXTENSION_VERSION, &src, sizeof(src)) == ENTROPY_OK, "SRC extension rejected");
        EntropyHostServices host {}; host.struct_size = sizeof(host); host.abi_version = ENTROPY_MODULE_ABI_VERSION;
        require(common.create(&host, &handle) == ENTROPY_OK, "create failed");
        EntropyPrepareInfo prepare {};
        prepare.struct_size = sizeof(prepare); prepare.sample_rate = 192000; prepare.maximum_frames = maximumFrames;
        prepare.input_channels = channels; prepare.output_channels = channels;
        require(common.prepare(handle, &prepare) == ENTROPY_OK, "prepare failed");
    }
    ~LoadedModule() { if (handle != nullptr) common.destroy(handle); if (library != nullptr) FreeLibrary(library); }
};

EntropySrcConfig configure(LoadedModule& module, std::uint32_t engine, std::uint32_t sourceRate,
                           std::uint32_t destinationRate, std::uint32_t channels = 8,
                           std::uint32_t maximumFrames = 1024)
{
    EntropySrcConfig config {};
    config.struct_size = sizeof(config); config.engine = engine; config.source_rate = sourceRate;
    config.destination_rate = destinationRate; config.channels = channels; config.maximum_input_frames = maximumFrames;
    require(module.src.configure(module.handle, &config) == ENTROPY_OK, "configure failed");
    require(config.required_output_capacity >= maximumFrames, "capacity not reported"); return config;
}

void descriptorTest(LoadedModule& module)
{
    EntropyModuleDescriptor descriptor {}; descriptor.struct_size = sizeof(descriptor);
    require(module.common.get_descriptor(&descriptor) == ENTROPY_OK, "descriptor failed");
    require(descriptor.module_kind == ENTROPY_MODULE_SRC, "wrong module kind");
    require((descriptor.module_flags & ENTROPY_MODULE_FLAG_FP64) != 0, "FP64 flag absent");
    require(descriptor.max_input_channels == 16, "max channels mismatch");
    std::uint32_t count = 0;
    require(module.src.get_engine_count(&count) == ENTROPY_OK && count == 1, "engine count mismatch");
    constexpr std::array<const char*, 1> ids {{ "r8brain-reference" }};
    for (std::uint32_t i = 0; i < count; ++i)
    {
        EntropySrcEngineDescriptor engine {}; engine.struct_size = sizeof(engine);
        require(module.src.get_engine_descriptor(i, &engine) == ENTROPY_OK, "engine descriptor failed");
        require(engine.engine == i && std::strcmp(engine.identifier, ids[i]) == 0, "engine identity mismatch");
    }
    std::cout << "PASS ABI, FP64 descriptor and single stable r8brain identity\n";
}

void nativeEightChannelTest(LoadedModule& module)
{
    const auto config = configure(module, ENTROPY_SRC_R8BRAIN_REFERENCE, 192000, 192000);
    std::array<std::vector<double>, 8> input, output;
    std::array<const double*, 8> inputs {}; std::array<double*, 8> outputs {};
    for (std::size_t channel = 0; channel < input.size(); ++channel)
    {
        input[channel].resize(1024); output[channel].assign(config.required_output_capacity, 0.0);
        for (std::size_t frame = 0; frame < input[channel].size(); ++frame) input[channel][frame] = channel * 0.125 + frame * 1.0e-7;
        inputs[channel] = input[channel].data(); outputs[channel] = output[channel].data();
    }
    EntropySrcBlockF64 block {};
    block.struct_size = sizeof(block); block.channels = 8; block.input_frames = 1024;
    block.output_capacity = config.required_output_capacity; block.inputs = inputs.data(); block.outputs = outputs.data();
    require(module.src.process(module.handle, &block) == ENTROPY_OK, "native process failed");
    require(block.consumed_frames == 1024 && block.produced_frames == 1024, "native frame count mismatch");
    for (std::size_t channel = 0; channel < input.size(); ++channel)
        require(std::memcmp(input[channel].data(), output[channel].data(), input[channel].size() * sizeof(double)) == 0, "native bypass is not bit exact");
    std::cout << "PASS bit-exact native-rate bypass on coherent CINEMA 8ch\n";
}

void resamplingEightChannelTest(LoadedModule& module, std::uint32_t engine)
{
    const auto config = configure(module, engine, 48000, 192000);
    std::array<std::vector<double>, 8> input, output;
    std::array<const double*, 8> inputs {}; std::array<double*, 8> outputs {};
    for (std::size_t channel = 0; channel < input.size(); ++channel)
    {
        input[channel].resize(1024); output[channel].assign(config.required_output_capacity, 0.0);
        inputs[channel] = input[channel].data(); outputs[channel] = output[channel].data();
    }
    EntropySrcBlockF64 block {};
    block.struct_size = sizeof(block); block.channels = 8; block.input_frames = 1024;
    block.output_capacity = config.required_output_capacity; block.inputs = inputs.data(); block.outputs = outputs.data();
    std::array<double, 8> energy {};
    std::uint64_t totalProduced = 0;
    for (std::uint32_t inputBlock = 0; inputBlock < 32; ++inputBlock)
    {
        for (std::size_t channel = 0; channel < input.size(); ++channel)
        {
            const double frequency = 997.0 + channel * 311.0;
            for (std::size_t frame = 0; frame < input[channel].size(); ++frame)
            {
                const auto absoluteFrame = static_cast<std::uint64_t>(inputBlock) * 1024 + frame;
                input[channel][frame] = 0.2 * std::sin(2.0 * pi * frequency * absoluteFrame / 48000.0);
            }
        }
        require(module.src.process(module.handle, &block) == ENTROPY_OK, "8ch process failed");
        require(block.consumed_frames == 1024, "8ch input was not consumed");
        totalProduced += block.produced_frames;
        for (std::size_t channel = 0; channel < output.size(); ++channel)
            for (std::uint32_t frame = 0; frame < block.produced_frames; ++frame)
            {
                require(std::isfinite(output[channel][frame]), "non-finite output");
                energy[channel] += output[channel][frame] * output[channel][frame];
            }
    }
    require(totalProduced > 0, "8ch conversion produced no data after startup latency");
    for (double value : energy) require(value > 1.0e-9, "silent channel output");
}

void independenceTest(const char* path)
{
    LoadedModule tidal(path, 2, 512), cinema(path, 8, 512);
    configure(tidal, ENTROPY_SRC_R8BRAIN_REFERENCE, 44100, 192000, 2, 512);
    configure(cinema, ENTROPY_SRC_R8BRAIN_REFERENCE, 48000, 192000, 8, 512);
    EntropySrcConfig a {}; a.struct_size = sizeof(a); EntropySrcConfig b {}; b.struct_size = sizeof(b);
    require(tidal.src.get_config(tidal.handle, &a) == ENTROPY_OK && cinema.src.get_config(cinema.handle, &b) == ENTROPY_OK, "independent get_config failed");
    require(a.engine == ENTROPY_SRC_R8BRAIN_REFERENCE && a.destination_rate == 192000 && a.channels == 2 && b.engine == ENTROPY_SRC_R8BRAIN_REFERENCE && b.destination_rate == 192000 && b.channels == 8, "instances share mutable state");
    std::cout << "PASS independent TIDAL/AUX/CINEMA instance state and engine rates\n";
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "usage: SrcModuleTests <dll>"); LoadedModule module(argv[1]);
        descriptorTest(module);
        EntropySrcConfig unsupported{}; unsupported.struct_size=sizeof(unsupported);
        unsupported.engine=1; unsupported.source_rate=48000; unsupported.destination_rate=192000;
        unsupported.channels=8; unsupported.maximum_input_frames=1024;
        require(module.src.configure(module.handle,&unsupported)==ENTROPY_ERROR_INVALID_ARGUMENT,
                "removed engine accepted");
        std::cout << "PASS removed engine rejected by native ABI\n";
        nativeEightChannelTest(module);
        for (std::uint32_t engine = 0; engine < 1; ++engine) { resamplingEightChannelTest(module, engine); std::cout << "PASS coherent CINEMA 8ch conversion engine " << engine << "\n"; }
        independenceTest(argv[1]); return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
