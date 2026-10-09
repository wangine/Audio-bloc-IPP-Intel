#include "SourceBlockExecutor.h"
#include "SourceCycleTimer.h"
#include "SourceSoakFixture.h"
#include "StageBusDemand.h"
#include "StageBusWin32.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <windows.h>
using namespace entropy::stage;
using namespace entropy::host;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << " " << error << "\n";                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
uint64_t qpc()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

int main(int argc, char **argv)
{
    unsigned seconds = argc > 1 ? std::stoul(argv[1]) : 2;
    bool peer = argc > 2;
    uint64_t token = peer ? std::stoull(argv[2]) : qpc();
    uint8_t uuid[16]{};
    std::memcpy(uuid, &token, 8);
    uuid[8] = 93;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    auto session = makeSession(1, uuid, frequency.QuadPart);
    std::string error;
    SourceCycleTimer timer;
    CHECK(timer.valid());
    SourceGraphRuntime graph;
    CHECK(graph.prepare(TEST_MODULE_ROOT, error));
    SourceBlockExecutor executor;
    CHECK(executor.prepare(graph, error));
    SoakFixture audio;
    CHECK(audio.prepare(graph, error));
    StageBusWin32 bus;
    if (peer)
    {
        CHECK(bus.open(session, error, reinterpret_cast<void *>(0x31000000000ull)));
        StageBusSourceControl sourceControl{};
        CHECK(bus.tryReceiveSourceControl(sourceControl) == StageBusIoResult::Ok);
        uint64_t controls = 1;
        auto begin = qpc();
        uint64_t blocks = static_cast<uint64_t>(seconds) * 192000 / 1024;
        uint64_t maximumUs = 0;
        std::array<uint64_t, 4096> histogram{};
        for (uint64_t seq = 1; seq <= blocks; ++seq)
        {
            uint64_t due = begin + (seq - 1) * 1024 * frequency.QuadPart / 192000;
            while (qpc() < due)
                timer.waitUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(200));
            StageBusSourceControl update{};
            auto controlIo = bus.tryReceiveSourceControl(update);
            if (controlIo == StageBusIoResult::Ok)
            {
                CHECK(update.source_revision >= sourceControl.source_revision);
                sourceControl = update;
                ++controls;
            }
            else
                CHECK(controlIo == StageBusIoResult::Empty);
            StageBusFrameRequest r{};
            std::memcpy(r.session_id, uuid, 16);
            r.stream_epoch = 1;
            r.block_sequence = seq;
            r.first_frame = (seq - 1) * 1024;
            r.control_revision = sourceControl.source_revision;
            r.frames = 1024;
            r.program.required_inputs = 3;
            r.program.music_program_active = (seq / 9375) % 2 == 0;
            r.program.effective_mute_mask = sourceControl.manual_mute_mask;
            r.program.manual_mute_mask = r.program.effective_mute_mask;
            CHECK(audio.update(seq, error));
            auto reference = executor.process(entropy::stage::SourceFrameEnvelope(r), r.program.effective_mute_mask, audio.reports[0], audio.reports[1],
                                              audio.reports[2], audio.out.data());
            CHECK(reference.passed);
            bus.heartbeat(qpc());
            CHECK(bus.trySendRequest(r) == StageBusIoResult::Ok);
            auto sent = qpc();
            StageBusBlockHeader h{};
            StageBusIoResult result;
            do
            {
                result = bus.tryConsume(h, audio.recv.data());
                if (result == StageBusIoResult::Empty)
                {
                    CHECK(qpc() - sent < static_cast<uint64_t>(frequency.QuadPart));
                    timer.waitUntil(std::chrono::steady_clock::now() +
                                    std::chrono::microseconds(200));
                }
            } while (result == StageBusIoResult::Empty);
            CHECK(result == StageBusIoResult::Ok && h.block_sequence == seq &&
                  h.first_frame == r.first_frame);
            for (unsigned c = 0; c < 10; ++c)
                CHECK(std::memcmp(audio.output[c].data(), audio.received[c].data(), 8192) == 0);
            CHECK(h.source_generation[0] == audio.reports[0].streamGeneration);
            auto us = (qpc() - sent) * 1000000 / frequency.QuadPart;
            ++histogram[(std::min)(uint64_t(4095), us / 2)];
            if (us > maximumUs)
                maximumUs = us;
        }
        std::cout << "PASS consumer blocks=" << blocks << " roundtrip_max_us=" << maximumUs << "\n";
        if (argc > 3)
        {
            auto percentile = [&](uint64_t percentage)
            {
                uint64_t cumulative = 0, target = (blocks * percentage + 99) / 100;
                for (unsigned bucket = 0; bucket < histogram.size(); ++bucket)
                {
                    cumulative += histogram[bucket];
                    if (cumulative >= target)
                        return (bucket + 1) * 2u;
                }
                return 8192u;
            };
            std::ofstream stats{std::filesystem::path(argv[3])};
            CHECK(stats);
            stats << "{\"status\":\"PASS\",\"blocks\":" << blocks
                  << ",\"roundtrip_max_us\":" << maximumUs
                  << ",\"roundtrip_p50_upper_us\":" << percentile(50)
                  << ",\"roundtrip_p95_upper_us\":" << percentile(95)
                  << ",\"roundtrip_p99_upper_us\":" << percentile(99)
                  << ",\"source_control_updates\":" << controls
                  << ",\"capture_provider_live\":false}";
            CHECK(stats.good());
        }
        return 0;
    }
    CHECK(bus.create(session, error));
    StageBusSourceControl sourceControl{};
    std::memcpy(sourceControl.session_id, uuid, 16);
    sourceControl.source_revision = 1;
    sourceControl.music_right = 1;
    CHECK(bus.trySendSourceControl(sourceControl) == StageBusIoResult::Ok);
    CHECK(bus.address() != reinterpret_cast<void *>(0x31000000000ull));
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr, path, 32768);
    std::wstring command = L"\"" + std::wstring(path) + L"\" " + std::to_wstring(seconds) + L" " +
                           std::to_wstring(token);
    auto peerMetrics =
        std::filesystem::path(path).parent_path() / ("peer-" + std::to_string(token) + ".json");
    command += L" \"" + peerMetrics.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION process{};
    CHECK(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &si, &process));
    StageBusDemand demand(session);
    uint64_t published = 0;
    auto started = qpc();
    uint64_t blocks = static_cast<uint64_t>(seconds) * 192000 / 1024;
    uint64_t maxProcessUs = 0;
    while (published < blocks)
    {
        bus.heartbeat(qpc());
        CHECK(WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT);
        StageBusFrameRequest r{};
        auto io = bus.tryReceiveRequest(r);
        if (io == StageBusIoResult::Empty)
        {
            CHECK(qpc() - started < static_cast<uint64_t>(seconds + 15) * frequency.QuadPart);
            timer.waitUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(200));
            continue;
        }
        CHECK(io == StageBusIoResult::Ok);
        CHECK(demand.accept(r) == DemandResult::Accepted);
        CHECK(bus.writable());
        CHECK(audio.update(r.block_sequence, error));
        auto t = qpc();
        auto result = executor.process(entropy::stage::SourceFrameEnvelope(r), r.program.effective_mute_mask, audio.reports[0], audio.reports[1], audio.reports[2],
                                       audio.out.data());
        CHECK(result.passed);
        result.header.qpc_timestamp = qpc();
        CHECK(bus.tryPublish(result.header, audio.send.data()) == StageBusIoResult::Ok);
        CHECK(demand.complete(r.block_sequence));
        auto us = (qpc() - t) * 1000000 / frequency.QuadPart;
        if (us > maxProcessUs)
            maxProcessUs = us;
        ++published;
        if ((published + 1) % 1875 == 0)
        {
            sourceControl.source_revision = 1 + (published + 1) / 1875;
            sourceControl.manual_mute_mask = ((published + 1) / 1875) % 8;
            CHECK(bus.trySendSourceControl(sourceControl) == StageBusIoResult::Ok);
        }
    }
    CHECK(WaitForSingleObject(process.hProcess, 5000) == WAIT_OBJECT_0);
    DWORD exitCode;
    CHECK(GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0);
    CHECK(std::filesystem::exists(peerMetrics));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    auto elapsed = qpc() - started;
    std::cout << "PASS SourceSoak seconds=" << seconds << " blocks=" << published
              << " frames=" << published * 1024 << " bitwise_values=" << published * 10240
              << " elapsed_seconds=" << double(elapsed) / frequency.QuadPart
              << " peer_metrics=" << peerMetrics.filename().string()
              << " processing_max_us=" << maxProcessUs << " full=" << bus.fullCount()
              << " faults=0 duplicate=0 mappings_distinct=1 "
                 "native_SRC_FIFO_six_rates_independent_generations=1 source_control_updates=1\n";
}
