#include "SourceDomainRuntime.h"
#include "SourceGraphRuntime.h"
#include <array>
#include <entropy/src_abi.h>
#include <iostream>
#include <vector>
#include <windows.h>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << " " << error << "\n";                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::host;
int main()
{
    std::string error;
    SourceGraphRuntime graph;
    CHECK(!graph.prepare(std::filesystem::path(TEST_MODULE_ROOT) / "missing", error));
    CHECK(graph.prepare(TEST_MODULE_ROOT, error));
    SourceDomainRuntime domain;
    CHECK(domain.prepareMusic(nullptr, graph, &graph, 0, 0, 0));
    CHECK(domain.auxPrepared() && domain.cinemaPrepared() && !domain.musicPrepared());
    domain.stop();
    for (const wchar_t *file :
         {L"EntropyCrossover.dll", L"EntropyCenterExtractor.dll", L"EntropyOutputMatrix.dll",
          L"EntropyEgressClockSync.dll", L"EntropyHeadroom.dll"})
        CHECK(!GetModuleHandleW(file));
    auto *src = graph.findInstance("src.aux");
    EntropySrcApiV1 api{};
    api.struct_size = sizeof(api);
    CHECK(src->queryExtension(ENTROPY_SRC_EXTENSION_ID, ENTROPY_SRC_EXTENSION_VERSION, &api,
                              sizeof(api), error));
    std::array<std::array<double, 256>, 2> in{};
    std::array<std::vector<double>, 2> out;
    const double *ip[2] = {in[0].data(), in[1].data()};
    double *op[2]{};
    in[0][0] = 1;
    in[1][0] = -.5;
    for (uint32_t rate : {44100u, 48000u, 88200u, 96000u, 176400u, 192000u})
    {
        EntropySrcConfig c{};
        c.struct_size = sizeof(c);
        c.engine = 0;
        c.source_rate = rate;
        c.destination_rate = 192000;
        c.channels = 2;
        c.maximum_input_frames = 256;
        CHECK(api.configure(src->handle(), &c) == ENTROPY_OK);
        CHECK(c.required_output_capacity > 0 && c.required_output_capacity < 1048576);
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            out[channel].assign(c.required_output_capacity, 0);
            op[channel] = out[channel].data();
        }
        CHECK(src->reset(error));
        uint64_t produced = 0;
        for (unsigned n = 0; n < 100; ++n)
        {
            EntropySrcBlockF64 b{};
            b.struct_size = sizeof(b);
            b.channels = 2;
            b.input_frames = 256;
            b.output_capacity = c.required_output_capacity;
            b.inputs = ip;
            b.outputs = op;
            CHECK(api.process(src->handle(), &b) == ENTROPY_OK);
            CHECK(b.consumed_frames == 256 && b.produced_frames <= c.required_output_capacity);
            produced += b.produced_frames;
        }
        CHECK(produced > 0);
        std::cout << "RATE " << rate << " produced=" << produced << "\n";
    }
    std::cout << "PASS Source isolation, resolver prepare without capture, native SRC six rates; "
                 "no downstream DLLs\n";
}
