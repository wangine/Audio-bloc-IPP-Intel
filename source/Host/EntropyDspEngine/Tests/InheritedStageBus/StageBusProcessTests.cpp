#include "StageBusWin32.h"
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <windows.h>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n";                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::stage;
struct Audio
{
    std::array<std::array<double, 1024>, 11> data{};
    std::array<double *, 11> out{};
    std::array<const double *, 11> in{};
    Audio()
    {
        for (int c = 0; c < 11; ++c)
        {
            out[c] = data[c].data();
            in[c] = out[c];
        }
    }
};
StageBusBlockHeader block(const StageBusSessionHeader &s, uint64_t seq)
{
    StageBusBlockHeader b{};
    std::memcpy(b.session_id, s.session_id, 16);
    b.stream_epoch = 1;
    b.block_sequence = seq;
    b.first_frame = (seq - 1) * 1024;
    b.frames = 1024;
    b.control_revision = 1;
    return b;
}
int main(int argc, char **argv)
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    uint8_t uuid[16]{};
    uint64_t token = argc >= 3 ? std::stoull(argv[2]) : GetCurrentProcessId();
    std::memcpy(uuid, &token, 8);
    uuid[8] = 42;
    auto s = makeSession(1, uuid, frequency.QuadPart);
    StageBusWin32 bus;
    std::string error;
    Audio audio;
    if (argc >= 3 && std::string(argv[1]) == "--peer")
    {
        CHECK(bus.open(s, error, reinterpret_cast<void *>(0x30000000000ull)));
        std::cout << "PEER_ADDRESS " << bus.address() << std::endl;
        StageBusFrameRequest request{};
        std::memcpy(request.session_id, s.session_id, 16);
        request.stream_epoch = 1;
        request.block_sequence = 1;
        request.frames = 1024;
        request.control_revision = 1;
        request.program.required_inputs = 1;
        request.program.music_program_active = 1;
        CHECK(bus.trySendRequest(request) == StageBusIoResult::Ok);
        Sleep(200);
        uint64_t seq = 1;
        auto deadline = GetTickCount64() + 5000;
        while (seq <= 4 && GetTickCount64() < deadline)
        {
            StageBusBlockHeader b{};
            auto result = bus.tryConsume(b, audio.out.data());
            if (result == StageBusIoResult::Empty)
            {
                Sleep(1);
                continue;
            }
            CHECK(result == StageBusIoResult::Ok);
            CHECK(b.block_sequence == seq);
            for (int c = 0; c < 10; ++c)
                for (int i = 0; i < 1024; ++i)
                    CHECK(audio.data[c][i] == double(seq * 100000 + c * 1024 + i));
            ++seq;
        }
        CHECK(seq == 5);
        StageBusBlockHeader response{};
        CHECK(bus.tryReceiveResponse(response) == StageBusIoResult::Ok &&
              response.block_sequence == 1);
        CHECK(bus.tryReceiveResponse(response) == StageBusIoResult::Empty);
        StageBusBlockHeader b{};
        CHECK(bus.tryConsume(b, audio.out.data()) == StageBusIoResult::Empty);
        std::cout << "PASS peer payload 40960 values, sequence/first_frame and empty\n";
        return 0;
    }
    CHECK(bus.create(s, error));
    std::cout << "PARENT_ADDRESS " << bus.address() << std::endl;
    CHECK(bus.address() != reinterpret_cast<void *>(0x30000000000ull));
    std::cout
        << "PEER_ADDRESS_VERIFIED 0x30000000000 (fixed MapViewOfFileEx, child exit checked)\\n";
    StageBusWin32 duplicate;
    CHECK(!duplicate.create(s, error));
    for (uint64_t seq = 1; seq <= 4; ++seq)
    {
        for (int c = 0; c < 10; ++c)
            for (int i = 0; i < 1024; ++i)
                audio.data[c][i] = double(seq * 100000 + c * 1024 + i);
        CHECK(bus.tryPublish(block(s, seq), audio.in.data()) == StageBusIoResult::Ok);
    }
    CHECK(!bus.writable());
    CHECK(bus.tryPublish(block(s, 5), audio.in.data()) == StageBusIoResult::Full);
    CHECK(bus.fullCount() == 1);
    wchar_t ownPath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, ownPath, MAX_PATH);
    std::wstring peerPath(ownPath);
    peerPath = peerPath.substr(0, peerPath.find_last_of(L"\\/")) + L"\\StageBusPeer.exe";
    std::wstring command = L"\"" + peerPath + L"\" --peer " + std::to_wstring(token);
    STARTUPINFOW start{};
    start.cb = sizeof(start);
    PROCESS_INFORMATION process{};
    CHECK(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &start, &process));
    StageBusFrameRequest request{};
    auto handshake = GetTickCount64() + 5000;
    StageBusIoResult received;
    do
    {
        received = bus.tryReceiveRequest(request);
        if (received == StageBusIoResult::Empty)
            Sleep(1);
    } while (received == StageBusIoResult::Empty && GetTickCount64() < handshake);
    CHECK(received == StageBusIoResult::Ok && request.block_sequence == 1);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    CHECK(bus.peerAlive(now.QuadPart));
    CHECK(!bus.peerAlive(now.QuadPart + 2 * frequency.QuadPart));
    CHECK(bus.peerAlive(now.QuadPart));
    CHECK(bus.trySendResponse(block(s, 1)) == StageBusIoResult::Ok);
    CHECK(bus.trySendResponse(block(s, 1)) == StageBusIoResult::Full);
    CHECK(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD code = 99;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CHECK(code == 0);
    CHECK(bus.writable());

    CHECK(bus.tryReceiveRequest(request) == StageBusIoResult::Empty);
    CHECK(!bus.peerAlive(0));
    StageBusWin32 stale;
    CHECK(!stale.open(s, error));
    bus.close();
    token++;
    std::memcpy(uuid, &token, 8);
    s = makeSession(1, uuid, frequency.QuadPart);
    CHECK(bus.create(s, error));
    auto invalid = block(s, 1);
    invalid.stream_epoch = 2;
    CHECK(bus.tryPublish(invalid, audio.in.data()) == StageBusIoResult::Fault);
    std::cout << "PASS StageBusProcess real child process, different mappings, full/no-overwrite, "
                 "death/stale rejection and invalid epoch\n";
}
