#pragma once
#include "SourceBlockExecutor.h"
#include "SourceFifo192.h"
#include "RealtimeBufferPolicy.h"
#include <entropy/src_abi.h>
#include <array>
#include <vector>
namespace entropy::host
{
// Offline oracle only: original SRC/FIFO API calls with deterministic source packets.
// Reconfiguration is a fixture control operation; it is excluded from steady-state allocation
// claims.
struct SoakFixture
{
    std::array<std::array<double, 1280>, 12> input{};
    std::array<std::array<double, 1024>, 10> output{}, received{};
    std::array<double *, 10> out{}, recv{};
    std::array<const double *, 10> send{};
    AudioBlockReport reports[3]{};
    std::unique_ptr<ModuleInstance> musicSrc;
    ModuleInstance *instances[3]{};
    EntropySrcApiV1 apis[3]{};
    EntropySrcConfig configs[3]{};
    SourceFifo192 fifos[3];
    std::array<std::array<std::vector<double>, 8>, 3> converted;
    std::array<std::array<double *, 8>, 3> cp{};
    std::array<std::array<const double *, 8>, 3> fp{};
    std::array<std::array<const double *, 8>, 3> ip{};
    std::array<std::array<double *, 8>, 3> pp{};
    uint64_t generations[3]{};
    bool prepare(SourceGraphRuntime &graph, std::string &error)
    {
        auto lib = ModuleLibrary::load(
            std::filesystem::path(TEST_MODULE_ROOT) / "EntropySampleRateConverter.dll", error);
        if (!lib)
            return false;
        musicSrc = std::make_unique<ModuleInstance>(lib);
        if (!musicSrc->create(error) || !musicSrc->prepare(192000, 1024, 2, 2, error))
            return false;
        instances[0] = musicSrc.get();
        instances[1] = graph.findInstance("src.aux");
        instances[2] = graph.findInstance("src.cinema");
        for (unsigned c = 0; c < 12; ++c)
            for (unsigned f = 0; f < 1024; ++f)
                input[c][f] = (c + 1) * .125 + f / 4096.;
        for (unsigned c = 0; c < 10; ++c)
        {
            out[c] = output[c].data();
            send[c] = out[c];
            recv[c] = received[c].data();
        }
        for (unsigned r = 0; r < 3; ++r)
        {
            unsigned channels = r == 2 ? 8 : 2, offset = r == 2 ? 4 : r * 2;
            apis[r].struct_size = sizeof(EntropySrcApiV1);
            if (!instances[r]->queryExtension(ENTROPY_SRC_EXTENSION_ID,
                                              ENTROPY_SRC_EXTENSION_VERSION, &apis[r],
                                              sizeof(apis[r]), error) ||
                !fifos[r].prepare(channels, RealtimeBufferPolicy::sourceCapacityFrames))
                return false;
            for (unsigned c = 0; c < channels; ++c)
            {
                ip[r][c] = input[offset + c].data();
                pp[r][c] = input[offset + c].data() + 256;
                reports[r].convertedAudioPointers[c] = pp[r][c];
            }
        }
        return true;
    }
    bool update(uint64_t sequence, std::string &error)
    {
        static constexpr uint32_t rates[6] = {44100, 48000, 88200, 96000, 176400, 192000};
        for (unsigned r = 0; r < 3; ++r)
        {
            uint64_t step = (sequence + r * 625) / 1875;
            uint64_t generation = 1 + step;
            unsigned channels = r == 2 ? 8 : 2;
            SourceStreamDescriptor descriptor{generation, rates[(step + r) % 6], channels, 64, 64,
                                              1};
            if (generations[r] != generation)
            {
                auto &cfg = configs[r];
                cfg = {};
                cfg.struct_size = sizeof(cfg);
                cfg.engine = 0;
                cfg.source_rate = descriptor.source_rate;
                cfg.destination_rate = 192000;
                cfg.channels = channels;
                cfg.maximum_input_frames = 256;
                if (apis[r].configure(instances[r]->handle(), &cfg) != ENTROPY_OK ||
                    !cfg.required_output_capacity || cfg.required_output_capacity > 1048576)
                {
                    error = "Fixture native SRC reconfigure rejected";
                    return false;
                }
                for (unsigned c = 0; c < channels; ++c)
                {
                    converted[r][c].assign(cfg.required_output_capacity, 0);
                    cp[r][c] = converted[r][c].data();
                    fp[r][c] = cp[r][c];
                }
                if (!fifos[r].reset(descriptor))
                {
                    error = "Fixture FIFO generation reset rejected";
                    return false;
                }
                generations[r] = generation;
            }
            unsigned budget = 0;
            while (fifos[r].telemetry().fill_frames < RealtimeBufferPolicy::sourceTargetFrames)
            {
                if (++budget > 1024)
                {
                    error = "Fixture SRC produced no bounded preroll";
                    return false;
                }
                EntropySrcBlockF64 b{};
                b.struct_size = sizeof(b);
                b.channels = channels;
                b.input_frames = 256;
                b.output_capacity = configs[r].required_output_capacity;
                b.inputs = ip[r].data();
                b.outputs = cp[r].data();
                if (apis[r].process(instances[r]->handle(), &b) != ENTROPY_OK ||
                    b.consumed_frames != 256)
                {
                    error = "Fixture native SRC process failed";
                    return false;
                }
                if (b.produced_frames &&
                    !fifos[r].push(fp[r].data(), b.produced_frames, descriptor))
                {
                    error = "Fixture converted FIFO overflow";
                    return false;
                }
            }
            SourceStreamDescriptor popped;
            if (!fifos[r].pop(pp[r].data(), 1024, popped) || popped != descriptor)
            {
                error = "Fixture FIFO identity/payload mismatch";
                return false;
            }
            reports[r].passed = true;
            reports[r].convertedAudioPointerFrames = 1024;
            reports[r].streamGeneration = popped.generation;
            reports[r].sourceRate = popped.source_rate;
            reports[r].state = 2;
            reports[r].channels = channels;
        }
        return true;
    }
};
} // namespace entropy::host
