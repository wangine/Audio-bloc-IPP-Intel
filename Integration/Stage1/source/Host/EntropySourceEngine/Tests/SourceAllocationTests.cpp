#include "RealtimeAllocationAudit.h"
#include "SourceBlockExecutor.h"
#include "SourceFifo192.h"
#include "StageBusWin32.h"
#include <array>
#include <chrono>
#include <cstring>
#include <entropy/src_abi.h>
#include <iostream>
#include <vector>
#include <windows.h>
using namespace entropy::host;
using namespace entropy::stage;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << " " << error << "\n";                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main()
{
    std::string error;
    SourceGraphRuntime graph;
    CHECK(graph.prepare(TEST_MODULE_ROOT, error));
    SourceBlockExecutor executor;
    CHECK(executor.prepare(graph, error));
    auto library = ModuleLibrary::load(
        std::filesystem::path(TEST_MODULE_ROOT) / "EntropySampleRateConverter.dll", error);
    CHECK(library);
    ModuleInstance musicSrc(library);
    CHECK(musicSrc.create(error) && musicSrc.prepare(192000, 1024, 2, 2, error));
    auto transport = ModuleLibrary::load(
        std::filesystem::path(TEST_MODULE_ROOT) / "EntropyMusicTransportEndpoint.dll", error);
    auto tidal = ModuleLibrary::load(
        std::filesystem::path(TEST_MODULE_ROOT) / "EntropyTidalEndpoint.dll", error);
    CHECK(transport && tidal);
    ModuleInstance *instances[3] = {&musicSrc, graph.findInstance("src.aux"),
                                    graph.findInstance("src.cinema")};
    EntropySrcApiV1 apis[3]{};
    EntropySrcConfig configs[3]{};
    SourceFifo192 fifos[3];
    std::array<std::array<double, 256>, 8> input{};
    for (unsigned c = 0; c < 8; ++c)
        for (unsigned i = 0; i < 256; ++i)
            input[c][i] = (c + 1) * .125;
    std::array<const double *, 8> ip{};
    for (unsigned c = 0; c < 8; ++c)
        ip[c] = input[c].data();
    std::array<std::array<std::vector<double>, 8>, 3> converted;
    std::array<std::array<double *, 8>, 3> cp{};
    std::array<std::array<const double *, 8>, 3> fp{};
    std::array<std::array<std::array<double, 1024>, 8>, 3> popped{};
    std::array<std::array<double *, 8>, 3> pp{};
    SourceStreamDescriptor descriptors[3]{};
    for (unsigned r = 0; r < 3; ++r)
    {
        apis[r].struct_size = sizeof(EntropySrcApiV1);
        CHECK(instances[r]->queryExtension(ENTROPY_SRC_EXTENSION_ID, ENTROPY_SRC_EXTENSION_VERSION,
                                           &apis[r], sizeof(apis[r]), error));
        auto &c = configs[r];
        c.struct_size = sizeof(c);
        c.engine = 0;
        c.source_rate = r == 0 ? 44100 : 48000;
        c.destination_rate = 192000;
        c.channels = r == 2 ? 8 : 2;
        c.maximum_input_frames = 256;
        CHECK(apis[r].configure(instances[r]->handle(), &c) == ENTROPY_OK);
        for (unsigned n = 0; n < c.channels; ++n)
        {
            converted[r][n].assign(c.required_output_capacity, 0);
            cp[r][n] = converted[r][n].data();
            fp[r][n] = cp[r][n];
            pp[r][n] = popped[r][n].data();
        }
        CHECK(fifos[r].prepare(c.channels, 524288));
        descriptors[r] = {1, c.source_rate, c.channels, 64, 64, 1};
        CHECK(fifos[r].reset(descriptors[r]));
    }
    uint8_t uuid[16]{};
    auto token = GetCurrentProcessId();
    std::memcpy(uuid, &token, sizeof(token));
    uuid[8] = 61;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    auto session = makeSession(1, uuid, frequency.QuadPart);
    StageBusWin32 producer, consumer;
    CHECK(producer.create(session, error) && consumer.open(session, error));
    std::array<std::array<double, 1024>, 10> output{}, sink{};
    double *op[10]{}, *sp[10]{};
    const double *send[10]{};
    for (unsigned c = 0; c < 10; ++c)
    {
        op[c] = output[c].data();
        sp[c] = sink[c].data();
        send[c] = op[c];
    }
    using Clock = std::chrono::steady_clock;
    std::array<double, 6> sumUs{}, maxUs{};
    std::array<uint64_t, 6> samples{};
    auto measure = [&](unsigned phase, Clock::time_point begin, uint64_t seq)
    {
        if (seq <= 100)
            return;
        const auto us = std::chrono::duration<double, std::micro>(Clock::now() - begin).count();
        sumUs[phase] += us;
        maxUs[phase] = (std::max)(maxUs[phase], us);
        ++samples[phase];
    };
    auto cycle = [&](uint64_t seq)
    {
        bool ok = true;
        AudioBlockReport reports[3]{};
        for (unsigned r = 0; r < 3; ++r)
        {
            const auto &c = configs[r];
            EntropySrcBlockF64 b{};
            b.struct_size = sizeof(b);
            b.channels = c.channels;
            b.input_frames = 256;
            b.output_capacity = c.required_output_capacity;
            b.inputs = ip.data();
            b.outputs = cp[r].data();
            auto started = Clock::now();
            ok &= apis[r].process(instances[r]->handle(), &b) == ENTROPY_OK;
            measure(r, started, seq);
            started = Clock::now();
            if (b.produced_frames)
                ok &= fifos[r].push(fp[r].data(), b.produced_frames, descriptors[r]);
            SourceStreamDescriptor current;
            reports[r].passed = fifos[r].pop(pp[r].data(), 1024, current);
            measure(3, started, seq);
            reports[r].convertedAudioPointerFrames = reports[r].passed ? 1024 : 0;
            reports[r].streamGeneration = 1;
            reports[r].sourceRate = c.source_rate;
            for (unsigned n = 0; n < c.channels; ++n)
                reports[r].convertedAudioPointers[n] = pp[r][n];
        }
        StageBusFrameRequest request{};
        std::memcpy(request.session_id, uuid, 16);
        request.stream_epoch = 1;
        request.block_sequence = seq;
        request.first_frame = (seq - 1) * 1024;
        request.control_revision = 1;
        request.frames = 1024;
        request.program.required_inputs = 3;
        request.program.music_program_active = 1;
        auto started = Clock::now();
        auto result = executor.process(entropy::stage::SourceFrameEnvelope(request), request.program.effective_mute_mask, reports[0], reports[1], reports[2], op);
        measure(4, started, seq);
        started = Clock::now();
        ok &= result.passed;
        ok &= producer.tryPublish(result.header, send) == StageBusIoResult::Ok;
        StageBusBlockHeader h{};
        ok &= consumer.tryConsume(h, sp) == StageBusIoResult::Ok;
        measure(5, started, seq);
        for (unsigned c = 0; c < 10; ++c)
            ok &= std::memcmp(op[c], sp[c], 8192) == 0;
        return ok;
    };
    for (uint64_t seq = 1; seq <= 100; ++seq)
        CHECK(cycle(seq));
    CHECK(allocation_audit::selfTest());
    allocation_audit::reset();
    bool passed = true;
    for (uint64_t seq = 101; seq <= 1100; ++seq)
    {
        allocation_audit::CycleScope measured;
        allocation_audit::audioPhase();
        passed &= cycle(seq);
    }
    auto counts = allocation_audit::snapshot();
    allocation_audit::report();
    CHECK(passed);
    CHECK(counts.cppNew == 0 && counts.cppDelete == 0 && counts.heapAlloc == 0 &&
          counts.heapFree == 0 && counts.wait == 0 && counts.lock == 0 && counts.io == 0);
    const char *names[] = {"SRC_MUSIC_44100", "SRC_AUX_48000",       "SRC_CINEMA_48000",
                           "FIFO_push_pop",   "Matrix_ProgramMixer", "IPC_publish_consume_copy"};
    for (unsigned i = 0; i < 6; ++i)
        std::cout << "LATENCY " << names[i] << " mean_us=" << sumUs[i] / samples[i]
                  << " max_us=" << maxUs[i] << " samples=" << samples[i] << "\n";
    for (unsigned r = 0; r < 3; ++r)
    {
        uint32_t latency = 0;
        CHECK(apis[r].latency_output_frames &&
              apis[r].latency_output_frames(instances[r]->handle(), &latency) == ENTROPY_OK);
        auto fifo = fifos[r].telemetry();
        std::cout << "QUEUE role=" << r << " SRC_delay_frames=" << latency
                  << " fixture_fifo_fill=" << fifo.fill_frames
                  << " capacity=" << fifo.capacity_frames << "\n";
    }
    std::cout << "PASS 1000 steady-state native SRC/FIFO/Matrix/ProgramMixer/IPC cycles; zero "
                 "covered allocations/deallocations/waits/locks/disk calls; 10240000 values "
                 "bitwise. Coverage: loaded named Windows imports and host C++ operators; excludes "
                 "dynamic resolution/syscalls/new images.\n";
}
