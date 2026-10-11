#include "InputMatrix.h"
#include "MultiChannelSrcEngine.h"
#include "MusicPublisher.h"
#include "PcmDecode.h"
#include "ProviderRegistry.h"
#include "SourceDomain.h"
#include "StageLinkSecurity.h"
#include "StagePublishV2.h"
#include "stage1_control_v1.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <entropy/input_matrix_abi.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace entropy::music;
thread_local bool auditAllocations = false;
thread_local uint64_t allocations = 0, deallocations = 0;
void *operator new(size_t n) {
    if (auditAllocations)
        allocations++;
    if (auto *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept {
    if (auditAllocations && p)
        deallocations++;
    std::free(p);
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete(void *p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, size_t) noexcept { ::operator delete(p); }
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x))                                                                                            \
            throw std::runtime_error(std::string("FAIL line ") + std::to_string(__LINE__) + ": " #x);        \
    } while (0)
EntropyIngressFormatV2 pcmFormat(unsigned rate, unsigned bits, unsigned valid) {
    EntropyIngressFormatV2 f{};
    f.structSize = sizeof(f);
    f.sampleRate = rate;
    f.channels = 2;
    f.containerBits = bits;
    f.validBits = valid;
    f.blockAlign = bits / 8 * 2;
    f.averageBytesPerSecond = rate * f.blockAlign;
    f.channelMask = 3;
    f.formatTag = 0xfffe;
    f.representation = ENTROPY_REP_PCM;
    const unsigned char guid[] = {1, 0, 0, 0, 0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113};
    std::memcpy(f.subFormat, guid, 16);
    return f;
}
void pcmCase() {
    for (auto bits : {16u, 32u}) {
        auto f = pcmFormat(44100, bits, bits == 16 ? 16 : 24);
        std::array<double, 8> l{}, r{};
        for (unsigned boundary = 0; boundary < 2; boundary++) {
            std::array<int32_t, 8> values = {-32768, 32767, -1, 1234, 0, -1234, 32767, -32768};
            std::vector<std::byte> committed(4 * f.blockAlign);
            for (unsigned i = 0; i < 8; i++) {
                if (bits == 16) {
                    int16_t v = static_cast<int16_t>(values[(i + boundary) % 8]);
                    std::memcpy(committed.data() + i * 2, &v, 2);
                } else {
                    int32_t v = values[(i + boundary) % 8] * 65536;
                    std::memcpy(committed.data() + i * 4, &v, 4);
                }
            }
            const auto &received = committed;
            EntropyIngressPacketV2 p{};
            p.abiVersion = ENTROPY_PROVIDER_ABI_V2;
            p.structSize = sizeof(p);
            p.format = f;
            p.generation = 7;
            p.packetSequence = boundary + 1;
            p.firstSourceFrame = boundary * 4;
            p.frames = 4;
            p.payloadBytes = static_cast<uint32_t>(received.size());
            p.qpcFrequency = 10000000;
            p.flags = ENTROPY_PACKET_RAW_IDENTITY;
            CHECK(decodeStereo({p, received}, l.data(), r.data(), 8));
            for (unsigned i = 0; i < 4; i++) {
                CHECK(l[i] == values[(i * 2 + boundary) % 8] / 32768.0);
                CHECK(r[i] == values[(i * 2 + 1 + boundary) % 8] / 32768.0);
            }
            auto bad = p;
            bad.format.subFormat[0] = 3;
            CHECK(!decodeStereo({bad, received}, l.data(), r.data(), 8));
            bad = p;
            bad.format.blockAlign++;
            CHECK(!decodeStereo({bad, received}, l.data(), r.data(), 8));
            bad = p;
            bad.format.channelMask = 4;
            CHECK(!decodeStereo({bad, received}, l.data(), r.data(), 8));
            bad = p;
            bad.payloadBytes--;
            CHECK(!decodeStereo({bad, received}, l.data(), r.data(), 8));
            bad = p;
            bad.format.sampleRate = 384000;
            CHECK(!decodeStereo({bad, received}, l.data(), r.data(), 8));
            CHECK(!decodeStereo({p, received}, l.data(), r.data(), 3));
        }
    }
    for (auto rate : {44100u, 48000u, 88200u, 96000u, 176400u, 192000u}) {
        CHECK(validFormat(pcmFormat(rate, 16, 16)));
        CHECK(validFormat(pcmFormat(rate, 32, 24)));
    }
    CHECK(!validFormat(pcmFormat(48000, 32, 32)));
    CHECK(!validFormat(pcmFormat(48000, 24, 24)));
    EntropyIngressPacketV2 converted{};
    converted.abiVersion = ENTROPY_PROVIDER_ABI_V2;
    converted.structSize = sizeof(converted);
    converted.format = pcmFormat(192000, 32, 24);
    converted.format.representation = ENTROPY_REP_F64_PLANAR;
    converted.format.containerBits = converted.format.validBits = 64;
    converted.format.blockAlign = 16;
    converted.format.averageBytesPerSecond = 192000 * 16;
    std::memset(converted.format.subFormat, 0, 16);
    converted.frames = 2;
    converted.payloadBytes = 32;
    converted.generation = 1;
    converted.qpcFrequency = 1;
    converted.flags = ENTROPY_PACKET_METADATA_PARTIAL | ENTROPY_PACKET_RAW_IDENTITY;
    double payload[] = {.1, .2, -.3, .4}, left[2]{}, right[2]{};
    CHECK(!decodeStereo({converted, {reinterpret_cast<const std::byte *>(payload), sizeof(payload)}}, left,
                        right, 2));
    std::cout
        << "PASS PCM16/24-in-32 distinct stereo, boundaries, six rates, strict metadata and legacy honesty\n";
}
#include "Stage2Interop.h"
void architectureCase() {
    ProviderRegistry registry;
    CHECK(registry.discover(TEST_PROVIDER_DIR));
    CHECK(registry.find("synthetic.music"));
    CHECK(registry.find("synthetic.aux"));
    CHECK(registry.enumerate().size() == 8);
    auto negativeRoot = std::filesystem::path(TEST_EVIDENCE_DIR) /
                        ("negative-providers-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(negativeRoot);
    std::filesystem::copy_file(std::filesystem::path(TEST_PROVIDER_DIR) / "Syntheticmusic.dll",
                               negativeRoot / "First.dll", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(std::filesystem::path(TEST_PROVIDER_DIR) / "Syntheticmusic.dll",
                               negativeRoot / "Duplicate.dll",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(std::filesystem::path(TEST_BAD_PROVIDER_DIR) / "BadAbiFixture.dll",
                               negativeRoot / "BadAbi.dll",
                               std::filesystem::copy_options::overwrite_existing);
    ProviderRegistry negative;
    CHECK(negative.discover(negativeRoot));
    CHECK(negative.find("synthetic.music") && !negative.find("synthetic.bad-abi"));
    CHECK(negative.errors.size() == 2);
    SourceSelector selector(registry);
    CHECK(selector.select(1, {"synthetic.music", "fixture"}));
    auto music = selector.selected(1);
    CHECK(selector.select(2, {"synthetic.aux", "fixture"}));
    CHECK(selector.selected(1) == music);
    CHECK(!selector.select(1, {"missing.provider", "missing"}));
    CHECK(selector.selected(1).providerId == "missing.provider");
    ProviderRegistry actual;
    CHECK(actual.discover(TEST_ACTUAL_PROVIDER_DIR));
    CHECK(actual.find("windows.capture.v2"));
    CHECK(actual.find("legacy.music-transport"));
    CHECK(actual.find("legacy.private-tap"));
    CHECK(actual.find("legacy.aux"));
    MusicPublisher publisher;
    std::string error;
    auto pipe = L"\\\\.\\pipe\\Entropy.Stage1.Acceptance." + std::to_wstring(GetCurrentProcessId());
    CHECK(publisher.start(pipe, error));
    entropy::publish::Subscriber subscriber;
    auto connecting = std::async(std::launch::async, [&] { return subscriber.connect(pipe, error); });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (connecting.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
           std::chrono::steady_clock::now() < deadline) {
        publisher.service();
        Sleep(1);
    }
    CHECK(connecting.get());
    CHECK(subscriber.session().layout_id == 3 && subscriber.session().channels == 2);
    ProgramBlock program;
    program.audio.left.fill(.25);
    program.audio.right.fill(-.125);
    program.validity = 1;
    program.sources[0].generation = 42;
    program.sources[0].rate = 192000;
    publisher.publish(program);
    StereoBlock received;
    double *planes[] = {received.left.data(), received.right.data()};
    StageBusBlockHeader header{};
    CHECK(subscriber.read(header, planes) == entropy::publish::ReadResult::Block);
    CHECK(received.left == program.audio.left && received.right == program.audio.right);
    CHECK(header.source_generation[0] == 42 && header.source_generation[2] == 0 && header.validity_mask == 1);
    CHECK(header.first_frame == 0 && header.block_sequence == 1);
    auto identity = publisher.session();
    subscriber.close();
    for (unsigned i = 0; i < 40; i++)
        publisher.publish(program);
    CHECK(publisher.sequence() == 41);
    CHECK(!memcmp(identity.session_id, publisher.session().session_id, 16));
    publisher.stop();
    auto root =
        std::filesystem::path(TEST_EVIDENCE_DIR) / ("process-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    auto config = root / "candidate.ini", stop = root / "stop", diag = root / "diagnostic.json";
    std::filesystem::remove(stop);
    {
        std::ofstream f(config);
        f << "[music]\nproviderId=synthetic.music\nendpointId=fixture\nmode=1\n[aux]\n";
    }
    auto corePipe = pipe + L".core", controlPipe = pipe + L".control";
    wchar_t overrideCore[32768]{};
    GetEnvironmentVariableW(L"STAGE1_ACCEPTANCE_CORE", overrideCore, 32768);
    std::wstring coreExecutable = overrideCore[0] ? overrideCore : TEST_CORE_EXE;
    std::wstring command = L"\"" + coreExecutable + L"\" --config \"" + config.wstring() +
                           L"\" --provider-directory \"" TEST_PROVIDER_DIR_W L"\" --publish-endpoint \"" +
                           corePipe + L"\" --control-endpoint \"" + controlPipe + L"\" --stop-file \"" +
                           stop.wstring() + L"\" --diagnostic-file \"" + diag.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    CHECK(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                         &startup, &child));
    struct Cleanup {
        PROCESS_INFORMATION &p;
        ~Cleanup() {
            if (WaitForSingleObject(p.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(p.hProcess, 99);
                WaitForSingleObject(p.hProcess, 2000);
            }
            CloseHandle(p.hProcess);
            CloseHandle(p.hThread);
        }
    } cleanup{child};
    entropy::publish::Subscriber core;
    bool connected = false;
    for (unsigned n = 0; n < 40 && WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT; n++) {
        if (core.connect(corePipe, error)) {
            connected = true;
            break;
        }
        Sleep(25);
    }
    CHECK(connected);
    auto transact = [&](Stage1ControlRequest request) {
        request.magic = STAGE1_CONTROL_MAGIC;
        request.abiVersion = STAGE1_CONTROL_ABI;
        request.structSize = sizeof(request);
        // Publication starts before the control server. A missing pipe makes
        // WaitNamedPipe return immediately; wait for its creation explicitly.
        bool controlReady=false;
        for(unsigned attempt=0;attempt<80 && WaitForSingleObject(child.hProcess,0)==WAIT_TIMEOUT;attempt++) {
            if(WaitNamedPipeW(controlPipe.c_str(),25)) {controlReady=true;break;}
            Sleep(25);
        }
        CHECK(controlReady);
        HANDLE h = CreateFileW(controlPipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, nullptr);
        CHECK(h != INVALID_HANDLE_VALUE);
        Stage1ControlResponse response{};
        bool ok = entropy::link::transfer(h, true, &request, sizeof(request), 2000);
        Sleep(25); // The server must preserve even a delayed reader's response.
        ok=ok && entropy::link::transfer(h, false, &response, sizeof(response), 2000);
        if(ok) {uint32_t ack=STAGE1_CONTROL_ACK;entropy::link::transfer(h,true,&ack,sizeof(ack),2000);}
        CloseHandle(h);
        CHECK(ok);
        CHECK(response.magic == STAGE1_CONTROL_MAGIC && response.abiVersion == STAGE1_CONTROL_ABI &&
              response.structSize == sizeof(response));
        return response;
    };
    Stage1ControlRequest request{};
    auto before = transact(request);
    CHECK(before.status == 0);
    Sleep(50);
    auto after = transact(request);
    // Publication/control readiness precedes the first ingress packet. Wait
    // for that packet's metadata, rather than assuming it arrives in 50 ms.
    auto metadataDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(after.sources[0].rate==0 && after.sources[0].state!=4 &&
          std::chrono::steady_clock::now()<metadataDeadline) {
        Sleep(10);
        after=transact(request);
    }
    CHECK(after.blocksPublished > before.blocksPublished);
    CHECK(!memcmp(before.sessionId, after.sessionId, 16));
    CHECK(after.sources[0].validBits == 24 && after.sources[0].containerBits == 32 &&
          after.sources[0].rate == 192000);
    request.operation = 3;
    auto removedMute = transact(request);
    CHECK(removedMute.status != 0 && removedMute.controlRevision == after.controlRevision);
    request.operation = 999;
    auto invalid = transact(request);
    CHECK(invalid.status != 0 && invalid.controlRevision == after.controlRevision);
    request = {};
    request.operation = 1;
    request.catalogOffset = 32;
    auto page = transact(request);
    CHECK(page.status == 0);
    request = {};
    request.operation = 2;
    request.role = 2;
    request.mode = 1;
    strcpy_s(request.providerId, "synthetic.music");
    strcpy_s(request.endpointId, "fixture");
    auto duplicate = transact(request);
    CHECK(duplicate.status != 0 && duplicate.controlRevision == after.controlRevision);
    unsigned blocks = 0;
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (blocks < 6 && std::chrono::steady_clock::now() < deadline) {
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        core.heartbeat(q.QuadPart);
        auto result = core.read(header, planes);
        if (result == entropy::publish::ReadResult::Block) {
            if (header.validity_mask & 1) {
                blocks++;
                CHECK(received.left[100] == .125 && received.right[100] == -.0625);
            }
        } else if (result == entropy::publish::ReadResult::Gap) {
            auto session = core.session();
            core.close();
            CHECK(core.connect(corePipe, error));
            CHECK(!memcmp(session.session_id, core.session().session_id, 16));
        } else
            CHECK(result == entropy::publish::ReadResult::Empty);
        Sleep(1);
    }
    CHECK(blocks == 6);
    core.close();
    stage2Interop(corePipe, root, after.sessionId);
    request = {};
    auto continued = transact(request);
    CHECK(continued.blocksPublished > after.blocksPublished &&
          !memcmp(continued.sessionId, after.sessionId, 16));
    request.operation = 2;
    request.role = 2;
    request.mode = 1;
    strcpy_s(request.providerId, "missing.provider");
    strcpy_s(request.endpointId, "missing.endpoint");
    auto missing = transact(request);
    CHECK(missing.status == 2 && missing.sources[1].state == 3 &&
          !strcmp(missing.sources[1].endpointId, "missing.endpoint"));
    CHECK(missing.sources[0].generation == continued.sources[0].generation &&
          missing.musicRevision == continued.musicRevision);
    // A transport can deliver buffered 44.1k input in a burst. SRC expands each
    // 2048-frame packet to ~8916 frames; the worker must wait for the fixed
    // scheduler to drain the existing bounded FIFO, without dropping audio.
    request = {};
    request.operation = 2;
    request.role = 1;
    request.mode = 1;
    strcpy_s(request.providerId, "synthetic.music");
    strcpy_s(request.endpointId, "burst44");
    CHECK(transact(request).status == 0);
    Sleep(550);
    request = {};
    auto burst = transact(request);
    CHECK(burst.sources[0].state == 2 && burst.sources[0].rate == 44100);
    CHECK(burst.sources[0].packets >= 8 && burst.sources[0].framesIn >= 16384);
    CHECK(burst.sources[0].faults == 0 && burst.sources[0].overruns == 0);
    CHECK(burst.blocksPublished > continued.blocksPublished &&
          !memcmp(burst.sessionId, continued.sessionId, 16));
    for(const char* endpoint : {"idle-running","idle-unknown"}) {
        request={};request.operation=2;request.role=1;request.mode=1;
        strcpy_s(request.providerId,"synthetic.music");strcpy_s(request.endpointId,endpoint);
        CHECK(transact(request).status==0);
        Sleep(250);request={};auto idle=transact(request);
        CHECK(idle.sources[0].state==1 && idle.sources[0].fillFrames==0);
        Sleep(100);auto stillIdle=transact(request);
        CHECK(stillIdle.sources[0].underruns==idle.sources[0].underruns);
        Sleep(300);auto starved=transact(request);
        CHECK(starved.sources[0].faults==0);
        CHECK(starved.sources[0].underruns>stillIdle.sources[0].underruns);
    }
    std::ofstream(stop).put('1');
    CHECK(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0);
    DWORD exitCode = 99;
    GetExitCodeProcess(child.hProcess, &exitCode);
    CHECK(exitCode == 0);
    CHECK(std::filesystem::exists(diag));
    std::cout << "PASS discovery/selector, MUSIC2 publisher, headless process and versioned IPC independent "
                 "lifetime\n";
}
EntropyIngressPacketV2 convertedPacket(unsigned rate, uint64_t generation, unsigned frames) {
    EntropyIngressPacketV2 p{};
    p.abiVersion = ENTROPY_PROVIDER_ABI_V2;
    p.structSize = sizeof(p);
    p.generation = generation;
    p.frames = frames;
    p.payloadBytes = frames * 16;
    p.qpcFrequency = 10000000;
    p.flags = ENTROPY_PACKET_METADATA_PARTIAL;
    p.originalContainerBits = 16;
    p.originalValidBits = 16;
    p.format = pcmFormat(rate, 16, 16);
    auto &f = p.format;
    f.representation = ENTROPY_REP_F64_PLANAR;
    f.containerBits = f.validBits = 64;
    f.blockAlign = 16;
    f.averageBytesPerSecond = rate * 16;
    f.formatTag = 0;
    memset(f.subFormat, 0, 16);
    return p;
}
#include "LevelAcceptance.h"
#include "PeakMeterAcceptance.h"
void pipelineCase() {
    peakMeterAcceptance();
    levelAcceptance();
    SourceDomain music, aux;
    auto p = convertedPacket(192000, 1, 2048);
    std::vector<double> payload(4096);
    for (unsigned i = 0; i < 2048; i++) {
        payload[i] = .25 + i / 65536.0;
        payload[i + 2048] = -.125 - i / 32768.0;
    }
    IngressPacketView view{p, {reinterpret_cast<const std::byte *>(payload.data()), payload.size() * 8}};
    CHECK(music.prepare(p.format, 1, 2048));
    CHECK(aux.prepare(p.format, 1, 2048));
    CHECK(music.processPacket(view));
    CHECK(aux.processPacket(view));
    CHECK(music.telemetry().validBits == 16);
    CHECK(music.telemetry().startupFramesMeasured == 2048);
    CHECK(music.telemetry().startupPeakL >= payload[2047]);
    StereoBlock m, a, out;
    CHECK(music.pop1024(m));
    CHECK(aux.pop1024(a));
    for (unsigned i = 0; i < 1024; i++) {
        CHECK(m.left[i] == payload[i]);
        CHECK(m.right[i] == payload[i + 2048]);
    }
    InputMatrix matrix;
    matrix.process(m, a, out);
    for (unsigned i = 0; i < 1024; i++) {
        CHECK(out.left[i] == m.left[i] + a.left[i]);
        CHECK(out.right[i] == m.right[i] + a.right[i]);
    }
    matrix.process(m, a, out, 1);
    CHECK(out.left == m.left && out.right == m.right);
    auto auxGeneration = aux.telemetry().generation;
    p.generation = 2;
    CHECK(music.prepare(p.format, 2, 2048));
    CHECK(!music.pop1024(m));
    CHECK(aux.telemetry().generation == auxGeneration);
    CHECK(aux.pop1024(a));
    CHECK(music.processPacket(view));
    CHECK(music.pop1024(m));
    CHECK(m.status.generation == 2);
    for (unsigned i = 0; i < 20; i++)
        music.processPacket(view);
    CHECK(music.telemetry().overruns > 0);
    SourceDomain converted;
    auto low = convertedPacket(48000, 9, 1024);
    std::vector<double> srcInput(2048, .125);
    std::fill(srcInput.begin() + 1024, srcInput.end(), -.25);
    CHECK(converted.prepare(low.format, 9, 1024));
    entropy::src::MultiChannelSrcEngine reference;
    EntropySrcConfig cfg{};
    cfg.struct_size = sizeof(cfg);
    cfg.engine = ENTROPY_SRC_R8BRAIN_REFERENCE;
    cfg.source_rate = 48000;
    cfg.destination_rate = 192000;
    cfg.channels = 2;
    cfg.maximum_input_frames = 1024;
    CHECK(reference.configure(cfg));
    std::vector<double> l(reference.config().required_output_capacity), r(l.size());
    std::vector<double> expectedL, expectedR, gotL, gotR;
    for (unsigned n = 0; n < 4; n++) {
        const double *inputs[] = {srcInput.data(), srcInput.data() + 1024};
        double *outputs[] = {l.data(), r.data()};
        EntropySrcBlockF64 b{};
        b.struct_size = sizeof(b);
        b.channels = 2;
        b.input_frames = 1024;
        b.output_capacity = static_cast<uint32_t>(l.size());
        b.inputs = inputs;
        b.outputs = outputs;
        CHECK(reference.process(b) == ENTROPY_OK);
        expectedL.insert(expectedL.end(), l.begin(), l.begin() + b.produced_frames);
        expectedR.insert(expectedR.end(), r.begin(), r.begin() + b.produced_frames);
        CHECK(converted.processPacket(
            {low, {reinterpret_cast<const std::byte *>(srcInput.data()), srcInput.size() * 8}}));
        while (converted.pop1024(m)) {
            gotL.insert(gotL.end(), m.left.begin(), m.left.end());
            gotR.insert(gotR.end(), m.right.begin(), m.right.end());
        }
    }
    CHECK(gotL.size() > 0 && gotL.size() <= expectedL.size());
    CHECK(std::equal(gotL.begin(), gotL.end(), expectedL.begin()));
    CHECK(std::equal(gotR.begin(), gotR.end(), expectedR.begin()));
    SourceDomain gap;
    auto half = convertedPacket(192000, 99, 512);
    std::vector<double> partial(1024, .1);
    CHECK(gap.prepare(half.format, 99, 512));
    CHECK(
        gap.processPacket({half, {reinterpret_cast<const std::byte *>(partial.data()), partial.size() * 8}}));
    CHECK(!gap.pop1024(m));
    half.flags |= ENTROPY_PACKET_DISCONTINUITY;
    std::fill(partial.begin(), partial.end(), .3);
    CHECK(
        gap.processPacket({half, {reinterpret_cast<const std::byte *>(partial.data()), partial.size() * 8}}));
    CHECK(!gap.pop1024(m));
    half.flags = ENTROPY_PACKET_METADATA_PARTIAL;
    CHECK(
        gap.processPacket({half, {reinterpret_cast<const std::byte *>(partial.data()), partial.size() * 8}}));
    CHECK(gap.pop1024(m));
    CHECK(std::all_of(m.left.begin(), m.left.end(), [](double x) { return x == .3; }));
    CHECK(gap.telemetry().startupFramesMeasured == 1024);
    SourceDomain warmed;
    auto auditPacket = convertedPacket(192000, 17, 2048);
    CHECK(warmed.prepare(auditPacket.format, 17, 2048));
    IngressPacketView av{auditPacket,
                         {reinterpret_cast<const std::byte *>(payload.data()), payload.size() * 8}};
    CHECK(warmed.processPacket(av));
    while (warmed.pop1024(m)) {
    }
    allocations = deallocations = 0;
    auditAllocations = true;
    bool processingOk = true;
    for (unsigned i = 0; i < 100; i++) {
        processingOk &= warmed.processPacket(av);
        while (warmed.pop1024(m)) {
        };
    }
    auditAllocations = false;
    CHECK(processingOk && allocations == 0 && deallocations == 0);
    std::cout << "PASS native SRC differential, exact stereo bypass, independent generations, unity stereo "
                 "mix without mute and bounded FIFO\n";
}
int main(int argc, char **argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--raw-provider-registry") {
            ProviderRegistry actual;
            CHECK(actual.discover(argv[2]));
            auto* raw=actual.find("entropy.music.render.v2.raw");
            CHECK(raw && raw->api.info.capabilities==5);
            std::cout<<"PASS candidate registry loads the actual raw PCM + lifecycle Provider DLL\n";
            return 0;
        }
        CHECK(argc == 2);
        if (std::string(argv[1]) == "--pcm")
            pcmCase();
        else if (std::string(argv[1]) == "--architecture")
            architectureCase();
        else if (std::string(argv[1]) == "--pipeline")
            pipelineCase();
        else
            throw std::runtime_error("Unknown acceptance case");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
