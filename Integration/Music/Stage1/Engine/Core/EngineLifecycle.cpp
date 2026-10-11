#include "EngineLifecycle.h"
#include "Configuration.h"
#include "InputMatrix.h"
#include "MusicPublisher.h"
#include "SourceDomain.h"
#include "WindowsEndpointCatalog.h"
#include <avrt.h>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
namespace entropy::music {
struct EngineLifecycle::Impl {
    struct Slot {
        SourceDomain domain;
        LoadedProvider *provider{};
        void *handle{};
        std::thread worker;
        std::atomic<bool> stop{false};
        std::atomic<uint32_t> state{0};
    };
    ProviderRegistry registry;
    std::unique_ptr<WindowsEndpointCatalog> windows;
    std::vector<ProviderEndpoint> catalog;
    std::vector<WindowsEndpoint> devices;
    Configuration config;
    Slot slots[2];
    MusicPublisher publisher;
    InputMatrix matrix;
    std::thread scheduler;
    HANDLE schedulerStop = CreateEventW(nullptr, TRUE, FALSE, nullptr),
           timer = CreateWaitableTimerExW(nullptr, nullptr, 0x2, TIMER_ALL_ACCESS);
    std::mutex controlMutex;
    std::atomic<uint64_t> revision{1}, musicRevision{1}, clockFrequency{0}, clockLate{0};
    std::atomic<bool> running{false};
    ~Impl() {
        if (timer)
            CloseHandle(timer);
        CloseHandle(schedulerStop);
    }
    void stopSource(unsigned i) {
        auto &s = slots[i];
        s.stop = true;
        if (s.handle && s.provider)
            s.provider->api.cancel(s.handle);
        if (s.worker.joinable())
            s.worker.join();
        if (s.handle && s.provider)
            s.provider->api.close(s.handle);
        s.handle = nullptr;
        s.provider = nullptr;
        s.domain.invalidate();
        s.state = 0;
    }
    bool startSource(unsigned i) {
        auto &s = slots[i];
        auto &selected = config.sources[i];
        if (selected.key.providerId.empty() || selected.key.endpointId.empty()) {
            s.state = 0;
            return true;
        }
        const auto &other = config.sources[1 - i];
        if (slots[1 - i].handle && selected.key == other.key) {
            s.state = 3;
            return false;
        }
        auto it = std::find_if(catalog.begin(), catalog.end(), [&](const ProviderEndpoint &e) {
            return selected.key.providerId == e.provider->api.info.providerId &&
                   selected.key.endpointId == e.endpoint.endpointId && e.endpoint.eligible &&
                   (e.endpoint.roles & (1u << i)) && selected.mode < 32 &&
                   (e.endpoint.accessModes & (1u << selected.mode));
        });
        if (it == catalog.end()) {
            s.state = 3;
            return false;
        }
        s.provider = it->provider;
        if (s.provider->api.open(selected.key.endpointId.c_str(), selected.mode, &s.handle) != 1 ||
            !s.handle) {
            s.handle = nullptr;
            s.provider = nullptr;
            s.state = 3;
            return false;
        }
        s.stop = false;
        s.state = 1;
        try {
            s.worker = std::thread([&s] {
                CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                try {
                    std::vector<std::byte> payload(ENTROPY_MAX_PACKET_FRAMES * 16);
                    EntropyIngressFormatV2 format{};
                    uint64_t generation = 0;
                    while (!s.stop) {
                        EntropyIngressPacketV2 p{};
                        p.abiVersion = ENTROPY_PROVIDER_ABI_V2;
                        p.structSize = sizeof(p);
                        auto rc = s.provider->api.read(s.handle, &p, payload.data(),
                                                       static_cast<uint32_t>(payload.size()));
                        if (rc < 0) {
                            if (!s.stop)
                                s.state = 4;
                            break;
                        }
                        if (!rc) {
                            Sleep(1);
                            continue;
                        }
                        if (p.payloadBytes > payload.size() || !validFormat(p.format)) {
                            s.state = 4;
                            break;
                        }
                        // This worker is not the audio scheduler. Prepare/configuration may allocate here.
                        if (generation != p.generation || memcmp(&format, &p.format, sizeof(format))) {
                            if (!s.domain.prepare(p.format, p.generation)) {
                                s.state = 4;
                                break;
                            }
                            format = p.format;
                            generation = p.generation;
                        }
                        auto waitForSpace = +[](void *context) noexcept {
                            auto &slot = *static_cast<Slot *>(context);
                            if (slot.stop.load())
                                return false;
                            Sleep(1);
                            return !slot.stop.load();
                        };
                        if (!s.domain.processPacket({p, {payload.data(), p.payloadBytes}}, waitForSpace,
                                                    &s)) {
                            s.state = 4;
                            break;
                        }
                        s.state = 2;
                    }
                } catch (...) {
                    s.state = 4;
                }
                s.domain.invalidate();
                CoUninitialize();
            });
        } catch (...) {
            stopSource(i);
            s.state = 4;
            return false;
        }
        return true;
    }
    void tick() {
        StereoBlock music, aux;
        ProgramBlock block;
        bool validMusic = slots[0].domain.pop1024(music), validAux = slots[1].domain.pop1024(aux);
        if (!validMusic)
            music = {};
        if (!validAux)
            aux = {};
        matrix.process(music, aux, block.audio, (validMusic ? 1u : 0u) | (validAux ? 2u : 0u));
        block.sources[0] = music.status;
        block.sources[1] = aux.status;
        block.validity = (validMusic ? 1u : 0u) | (validAux ? 2u : 0u);
        block.discontinuity = (music.status.flags & 1 ? 1u : 0u) | (aux.status.flags & 1 ? 2u : 0u);
        block.controlRevision = revision;
        block.musicRevision = musicRevision;
        publisher.publish(block);
    }
    void runScheduler() {
        DWORD taskIndex = 0;
        auto mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        LARGE_INTEGER frequency, origin;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&origin);
        clockFrequency = frequency.QuadPart;
        uint64_t blocks = 0;
        while (WaitForSingleObject(schedulerStop, 0) != WAIT_OBJECT_0) {
            // Single publisher owner; service is a control phase, never concurrent with publish.
            publisher.service();
            tick();
            ++blocks;
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            auto target = origin.QuadPart + static_cast<int64_t>(static_cast<long double>(blocks) * 1024 *
                                                                 frequency.QuadPart / 192000);
            auto ticks = target - now.QuadPart;
            if (ticks <= 0)
                clockLate++;
            LARGE_INTEGER due;
            due.QuadPart = -((std::max)(int64_t(1), static_cast<int64_t>(static_cast<long double>(ticks) *
                                                                         10000000 / frequency.QuadPart)));
            if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                break;
            HANDLE waits[] = {schedulerStop, timer};
            if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                break;
        }
        if (mmcss)
            AvRevertMmThreadCharacteristics(mmcss);
        running = false;
    }
    void refresh() {
        catalog = registry.enumerate();
        devices = windows->enumerate();
    }
    Stage1ControlResponse status(uint32_t offset = 0) {
        Stage1ControlResponse result{};
        result.magic = STAGE1_CONTROL_MAGIC;
        result.abiVersion = STAGE1_CONTROL_ABI;
        result.structSize = sizeof(result);
        result.blocksPublished = publisher.sequence();
        result.publisherDrops = publisher.dropped();
        result.controlRevision = revision;
        result.musicRevision = musicRevision;
        result.clockQpcFrequency = clockFrequency;
        result.clockLateBlocks = clockLate;
        result.clockRunning = running;
        result.outputRate = 192000;
        memcpy(result.sessionId, publisher.session().session_id, 16);
        if (windows->changed())
            refresh();
        for (unsigned i = 0; i < 2; i++) {
            auto &t = result.sources[i];
            auto &s = slots[i];
            auto telemetry = s.domain.telemetry();
            strncpy_s(t.providerId, config.sources[i].key.providerId.c_str(), _TRUNCATE);
            strncpy_s(t.endpointId, config.sources[i].key.endpointId.c_str(), _TRUNCATE);
            t.state = s.state;
            t.representation = telemetry.representation;
            t.mode = telemetry.mode;
            t.rate = telemetry.rate;
            t.containerBits = telemetry.containerBits;
            t.validBits = telemetry.validBits;
            t.fillFrames = telemetry.fillFrames;
            t.metadataFlags = telemetry.metadataFlags;
            t.packetSequence = telemetry.packetSequence;
            t.firstSourceFrame = telemetry.firstSourceFrame;
            t.qpcTimestamp = telemetry.qpcTimestamp;
            t.qpcFrequency = telemetry.qpcFrequency;
            t.generation = telemetry.generation;
            t.packets = telemetry.packets;
            t.framesIn = telemetry.framesIn;
            t.framesOut = telemetry.framesOut;
            t.lostFrames = telemetry.lostFrames;
            t.overruns = telemetry.overruns;
            t.underruns = telemetry.underruns;
            t.faults = telemetry.faults;
            t.prePeakL = telemetry.prePeakL;
            t.prePeakR = telemetry.prePeakR;
            t.preRmsL = telemetry.preRmsL;
            t.preRmsR = telemetry.preRmsR;
            t.preMidRms = telemetry.preMidRms;
            t.preSideRms = telemetry.preSideRms;
            t.postPeakL = telemetry.postPeakL;
            t.postPeakR = telemetry.postPeakR;
            t.postRmsL = telemetry.postRmsL;
            t.postRmsR = telemetry.postRmsR;
            t.postMidRms = telemetry.postMidRms;
            t.postSideRms = telemetry.postSideRms;
            t.truePeakL = telemetry.truePeakL;
            t.truePeakR = telemetry.truePeakR;
            t.startupPeakL = telemetry.startupPeakL;
            t.startupPeakR = telemetry.startupPeakR;
            t.truePeakReady = telemetry.truePeakReady;
            t.startupComplete = telemetry.startupComplete;
            t.startupFramesMeasured = telemetry.startupFramesMeasured;
        }
        uint32_t position = 0;
        auto append = [&](const Stage1CatalogEntry &entry) {
            if (position++ < offset)
                return;
            if (result.endpointCount < STAGE1_CONTROL_ENDPOINTS)
                result.endpoints[result.endpointCount++] = entry;
            else
                result.truncated = 1;
        };
        for (auto &e : catalog) {
            Stage1CatalogEntry entry{};
            strcpy_s(entry.providerId, e.provider->api.info.providerId);
            entry.endpoint = e.endpoint;
            append(entry);
        }
        for (auto &e : devices) {
            if (e.id.size() >= 512 || e.name.size() >= 256)
                continue;
            Stage1CatalogEntry entry{};
            entry.endpoint.abiVersion = ENTROPY_PROVIDER_ABI_V2;
            entry.endpoint.structSize = sizeof(entry.endpoint);
            entry.endpoint.flow = e.flow;
            entry.endpoint.state = e.state;
            strcpy_s(entry.endpoint.endpointId, e.id.c_str());
            strcpy_s(entry.endpoint.windowsEndpointId, e.id.c_str());
            strcpy_s(entry.endpoint.name, e.name.c_str());
            append(entry);
        }
        result.catalogTotal = position;
        return result;
    }
};
EngineLifecycle::EngineLifecycle() : impl_(std::make_unique<Impl>()) {}
EngineLifecycle::~EngineLifecycle() { stop(); }
bool EngineLifecycle::start(const std::filesystem::path &config, const std::filesystem::path &providers,
                            const std::wstring &publish, std::string &error) {
    auto &p = *impl_;
    if (!p.timer || !p.schedulerStop) {
        error = "High resolution scheduler timer unavailable";
        return false;
    }
    p.config = Configuration::load(config);
    p.registry.discover(providers);
    p.windows = std::make_unique<WindowsEndpointCatalog>();
    p.refresh();
    if (!p.publisher.start(publish, error))
        return false;
    for (unsigned i = 0; i < 2; i++)
        p.startSource(i);
    p.running = true;
    p.scheduler = std::thread([&p] { p.runScheduler(); });
    return true;
}
void EngineLifecycle::stop() {
    auto &p = *impl_;
    SetEvent(p.schedulerStop);
    if (p.scheduler.joinable())
        p.scheduler.join();
    for (unsigned i = 0; i < 2; i++)
        p.stopSource(i);
    p.publisher.stop();
}
Stage1ControlResponse EngineLifecycle::snapshot() {
    std::lock_guard lock(impl_->controlMutex);
    return impl_->status();
}
Stage1ControlResponse EngineLifecycle::control(const Stage1ControlRequest &r) {
    auto &p = *impl_;
    std::lock_guard lock(p.controlMutex);
    bool invalid =
        r.magic != STAGE1_CONTROL_MAGIC || r.abiVersion != STAGE1_CONTROL_ABI || r.structSize != sizeof(r) ||
        r.operation > 2 || r.catalogOffset > 4096 || (r.operation != 1 && r.catalogOffset != 0) ||
        !memchr(r.providerId, 0, sizeof(r.providerId)) || !memchr(r.endpointId, 0, sizeof(r.endpointId));
    for (auto v : r.reserved)
        invalid |= v != 0;
    if (r.operation == 2 && (r.role != 1 && r.role != 2))
        invalid = true;
    if (r.operation == 2 && (r.mode >= 32 || (!r.providerId[0] != !r.endpointId[0])))
        invalid = true;
    if (!invalid && r.operation == 2 && r.providerId[0]) {
        const auto &other = p.config.sources[r.role == 2 ? 0 : 1].key;
        if (other.providerId == r.providerId && other.endpointId == r.endpointId)
            invalid = true;
    }
    uint32_t status = invalid ? 1 : 0;
    const char *message = invalid ? "Invalid control request" : "";
    if (!invalid && r.operation == 1 && !r.catalogOffset)
        p.refresh();
    if (!invalid && r.operation == 2) {
        unsigned i = r.role == 2 ? 1 : 0;
        p.stopSource(i);
        p.config.sources[i].key = {r.providerId, r.endpointId};
        p.config.sources[i].mode = r.mode;
        p.refresh();
        bool available = p.startSource(i);
        p.revision++;
        if (!i)
            p.musicRevision++;
        if (!available) {
            status = 2;
            message = "UNAVAILABLE; exact selection retained; no fallback";
        }
        if (!p.config.save()) {
            status = 3;
            message = "Configuration save failed";
        }
    }
    auto result = p.status(!invalid && r.operation == 1 ? r.catalogOffset : 0);
    result.requestId = r.requestId;
    result.status = status;
    strcpy_s(result.message, message);
    return result;
}
} // namespace entropy::music
