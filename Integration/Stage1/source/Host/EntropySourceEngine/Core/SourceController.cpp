#include "SourceController.h"
#include "EngineCadencePolicy.h"
#include "ProAudioThreadScope.h"
#include "ProgramSourcePoll.h"
#include "SourceModuleHashes.h"
#include "SourceCycleTimer.h"
#include "SourceFrameDemand.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <entropy/src_abi.h>
#include <tlhelp32.h>
#include <windows.h>
namespace entropy::host
{
namespace
{
uint64_t qpc() noexcept
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
QString path(const std::filesystem::path &p) { return QString::fromStdWString(p.wstring()); }
} // namespace
bool SourceController::verifyModules(const std::filesystem::path &root, std::string &error)
{
    error.clear();
    std::error_code ec;
    unsigned count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(root, ec))
    {
        if (entry.is_symlink())
        {
            error = "Source module symlinks are not allowed";
            return false;
        }
        if (QString::fromStdWString(entry.path().extension().wstring())
                .compare(".dll", Qt::CaseInsensitive) == 0)
            ++count;
    }
    if (ec || count != 6)
    {
        error = "Source module root must contain exactly six native R10 DLLs";
        return false;
    }
    for (const auto &expected : sourceModuleHashes)
    {
        QFile file(path(root / expected[0]));
        if (!file.open(QIODevice::ReadOnly))
        {
            error = "Required Source DLL missing";
            return false;
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file) || hash.result().toHex() != expected[1])
        {
            error = "Stage1 native module hash mismatch: " + std::string(expected[0]);
            return false;
        }
    }
    return true;
}
bool SourceController::baselineOwnsSources()
{
    HANDLE scan = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (scan == INVALID_HANDLE_VALUE)
        return true;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(scan, &entry))
        do
        {
            if (!_wcsicmp(entry.szExeFile, L"EntropyAudioEngine.exe"))
            {
                found = true;
                break;
            }
        } while (Process32NextW(scan, &entry));
    CloseHandle(scan);
    return found;
}
bool SourceController::start(const std::filesystem::path &root,
                             const std::filesystem::path &stateRoot, std::string &error,
                             bool offline)
{
    if (audio_.joinable() || control_.joinable())
    {
        error = "Source already started";
        return false;
    }
    if (!verifyModules(root, error))
        return false;
    HANDLE owner = CreateMutexW(nullptr, FALSE, L"Local\\Entropy.Stage1.Source.Owner.V1");
    if (!owner || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (owner)
            CloseHandle(owner);
        error = "Another Source process owns the Stage 1 capture lifecycle";
        return false;
    }
    ownerMutex_ = owner;
    modules_ = root;
    stateRoot_ = stateRoot;
    offline_ = offline;
    stopping_ = false;
    pause_ = paused_ = false;
    state_ = std::make_unique<SourceState>(stateRoot_);
    if (std::filesystem::exists(stateRoot_ / "SourceState.json"))
    {
        if (!state_->load(saved_, error))
        {
            stop();
            return false;
        }
        saved_.current = SourceStartupPolicy::musicKey();
        saved_.leftChannel = 0;
        saved_.rightChannel = 1;
    }
    saved_.engines = {0, 0, 2};
    manualMute_ = saved_.manualMuteMask;
    revision_ = saved_.revision;
    if (!graph_.prepare(modules_, error) || !executor_.prepare(graph_, error))
    {
        stop();
        return false;
    }
    auto *src = graph_.findInstance("src.aux");
    EntropySrcApiV1 api{};
    api.struct_size = sizeof(api);
    if (!src->queryExtension(ENTROPY_SRC_EXTENSION_ID, ENTROPY_SRC_EXTENSION_VERSION, &api,
                             sizeof(api), error) ||
        api.get_engine_count(&capabilityCount_) != ENTROPY_OK ||
        capabilityCount_ > capabilities_.size())
    {
        stop();
        error = "SRC capability table invalid";
        return false;
    }
    for (unsigned i = 0; i < capabilityCount_; ++i)
    {
        EntropySrcEngineDescriptor d{};
        d.struct_size = sizeof(d);
        if (api.get_engine_descriptor(i, &d) != ENTROPY_OK ||
            !std::memchr(d.name, 0, sizeof(d.name)) ||
            !std::memchr(d.identifier, 0, sizeof(d.identifier)))
        {
            stop();
            error = "SRC descriptor invalid";
            return false;
        }
        capabilities_[i].engine = d.engine;
        capabilities_[i].flags = d.flags;
        std::memcpy(capabilities_[i].identifier, d.identifier, sizeof(d.identifier));
        std::memcpy(capabilities_[i].name, d.name, sizeof(d.name));
    }
    for (unsigned c = 0; c < 10; ++c)
    {
        outPointers_[c] = output_[c].data();
        sendPointers_[c] = outPointers_[c];
        sinkPointers_[c] = localSink_[c].data();
    }
    if ((!publicationStarted_ && !enablePublication(publicationEndpoint_.empty() ? entropy::publish::sourceEndpointName() : publicationEndpoint_, error)) ||
        !newSession(false, error))
    {
        stop();
        return false;
    }
    reason_ = offline ? "OFFLINE — no capture; standalone demand harness"
                      : "Starting Source control worker";
    audio_ = std::thread([this] { audioWorker(); });
    control_ = std::thread([this] { controlWorker(); });
    return true;
}
bool SourceController::enableStageLink(std::wstring endpoint,std::string& error)
{
    if(audio_.joinable() || control_.joinable() || stageLink_){error="Enable control endpoint before Source starts";return false;}
    stageLink_=std::make_unique<entropy::link::StageLinkServer>(std::move(endpoint));
    if(!stageLink_->start(error)){stageLink_.reset();return false;}return true;
}
bool SourceController::enablePublication(std::wstring endpoint,std::string& error)
{
    if(audio_.joinable() || control_.joinable() || publicationStarted_)
    { error="Enable passive publication before Source starts"; return false; }
    if(!publisher_.start(endpoint,error))return false;
    publicationEndpoint_=std::move(endpoint);
    publicationStarted_=true;
    return true;
}
void SourceController::servicePublication()
{
    // Queue allocation, liveness checks and retirement belong to control, while
    // publication owns those same queues on audio. Pause establishes one owner.
    publicationPause_=true;
    if(pause()) { publisher_.service(); resume(); }
    publicationPause_=false;
}
void SourceController::serviceStageLink()
{
    using namespace entropy::link;
    if(!stageLink_)return;
    StageLinkRequest request{};uint32_t verifiedPid=0;
    if(!stageLink_->tryTake(request,verifiedPid))return;
    StageLinkResponse reply{};reply.sourcePid=GetCurrentProcessId();
    std::memcpy(reply.nonce,request.nonce,16);
    // V1 Query is diagnostic only. No V1 Attach/Release or external execution.
    reply.status=(request.action==StageLinkAction::Query && faulted_)
        ? StageLinkStatus::Fault : StageLinkStatus::Refused;
    reply.session=session_;reply.controlRevision=revision_;
    reply.program.required_inputs=3;reply.program.music_program_active=1;reply.program.analyze_music_final=1;
    reply.program.manual_mute_mask=reply.program.effective_mute_mask=manualMute_;
    stageLink_->complete(reply);
}
void SourceController::stop() noexcept
{
    stopping_ = true;
    pause_ = false;
    if (audio_.joinable())
        audio_.join();
    if (control_.joinable())
        control_.join();
    if(stageLink_)stageLink_->stop();
    publisher_.stop();
    publicationStarted_=false;
    domains_.stop();
    capturing_ = false;
    if (ownerMutex_)
    {
        CloseHandle(ownerMutex_);
        ownerMutex_ = nullptr;
    }
}
bool SourceController::command(SourceCommand cmd)
{
    std::lock_guard lock(mutex_);
    if (stopping_ || commands_.size() >= 16)
        return false;
    commands_.push_back(std::move(cmd));
    return true;
}
void SourceController::mute(uint32_t mask) noexcept
{
    try
    {
        SourceCommand cmd;
        cmd.type = SourceCommand::Mute;
        cmd.value = mask & 7;
        command(std::move(cmd));
    }
    catch (...)
    {
        ++faults_;
    }
}
bool SourceController::pause()
{
    if (!audio_.joinable())
        return true;
    pause_ = true;
    auto until = GetTickCount64() + 1000;
    while (!paused_ && !stopping_ && GetTickCount64() < until)
        Sleep(1);
    if (!paused_)
    {
        pause_ = false;
        return false;
    }
    return true;
}
void SourceController::resume() noexcept
{
    if (controlQuiesce_)
        return;
    pause_ = false;
    while (paused_.load() && !stopping_.load())
        Sleep(1);
}
SourceStateSnapshot SourceController::coherentState() const
{
    auto s = saved_;
    s.manualMuteMask = manualMute_.load();
    s.revision = revision_;
    if (capturing_)
    {
        auto m = domains_.musicTelemetry(), a = domains_.auxTelemetry(),
             c = domains_.cinemaTelemetry();
        if (domains_.musicPrepared())
            s.engines[0] = m.src_engine;
        if (domains_.auxPrepared())
            s.engines[1] = a.src_engine;
        if (domains_.cinemaPrepared())
            s.engines[2] = c.src_engine;
    }
    return s;
}
SourceUiSnapshot SourceController::snapshot() const
{
    std::lock_guard lock(mutex_);
    SourceUiSnapshot result;
    result.reason = faulted_ ? "SOURCE FAULT — local Retry required: "+reason_ : reason_;
    result.catalog = catalog_;
    result.state = coherentState();
    result.state.manualMuteMask = manualMute_;
    result.sourceControlPending = manualMute_.load() != appliedManualMute_.load();
    result.blocks = blocks_;
    result.faults = faults_;
    result.duplicates = duplicates_;
    result.publishDrops = publisher_.dropped();
    result.capturing = capturing_;
    result.external = external_;
    result.demandSourceMask = demandSourceMask_.load();
    result.sequence = lastSequence_;
    result.firstFrame = result.sequence ? (result.sequence - 1) * 1024 : 0;
    result.sources = {domains_.musicTelemetry(), domains_.auxTelemetry(),
                      domains_.cinemaTelemetry()};
    result.session = session_;
    result.outputValidity = static_cast<uint32_t>(lastValidity_.load());
    for (unsigned c = 0; c < 10; ++c)
        result.outputPeaks[c] = outputPeaks_[c].load();
    return result;
}
bool SourceController::telemetry(EngineTelemetrySnapshot &out) const
{
    std::lock_guard lock(mutex_);
    out = EngineTelemetrySnapshot{};
    out.srcCapabilityAvailable = 1;
    out.srcEngineCount = capabilityCount_;
    for (unsigned i = 0; i < capabilityCount_; ++i)
        out.srcEngines[i] = capabilities_[i];
    const std::array<SourceDomainTelemetry, 3> inputs = {
        domains_.musicTelemetry(), domains_.auxTelemetry(), domains_.cinemaTelemetry()};
    for (unsigned r = 0; r < 3; ++r)
    {
        const auto &t = inputs[r];
        auto &ui = out.inputs[r];
        ui.state = t.state;
        ui.bridgeConnected = capturing_ && t.state != 0;
        ui.sourceRate = t.source_rate;
        ui.channels = r == 2 ? 8 : 2;
        ui.validBits = t.valid_bits;
        ui.containerBits = t.container_bits;
        ui.livePathPassed = (lastValidity_.load() & (1u << r)) != 0;
        ui.muted = (manualMute_.load() & (1u << r)) != 0;
        ui.capturedFrames = static_cast<uint32_t>(t.endpoint_frames_read);
        ui.convertedFrames = static_cast<uint32_t>(t.src_output_frames);
        ui.srcEngine = capturing_ ? t.src_engine : saved_.engines[r];
        ui.srcLatencyFrames = t.src_latency_frames;
        ui.srcControlAvailable = 1;
        ui.endpointAvailableFrames = t.endpoint_available_frames;
        ui.sourceQueueDepthFrames = t.fifo.fill_frames;
        ui.generation = t.generation;
        ui.lostFrames = t.endpoint_lost_frames;
        ui.sourceFifoProducedFrames = t.fifo.produced_frames;
        ui.sourceFifoConsumedFrames = t.fifo.consumed_frames;
        ui.sourceFifoUnderruns = t.fifo.underruns;
        ui.sourceFifoOverflows = t.fifo.overflows;
        ui.sourceFifoCapacityFrames = t.fifo.capacity_frames;
        ui.sourceFifoHighWaterFrames = t.fifo.high_water_frames;
        ui.endpointPeak = (std::max)(t.peak_left, t.peak_right);
    }
    return true;
}
bool SourceController::newSession(bool, std::string &error)
{
    if (!pause())
    {
        error = "Source audio quiesce timeout";
        return false;
    }
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
    {
        error = "Session UUID generation failed";
        resume();
        return false;
    }
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    session_ =
        entropy::stage::makeSession(1, reinterpret_cast<uint8_t *>(&guid), frequency.QuadPart);
    harness_ = SourceDemandHarness{};
    session_.producer_pid=GetCurrentProcessId();
    if (!harness_.begin(session_, SourceDemandOwner::Standalone))
    {
        error="Local Source demand initialization failed";
        faulted_ = true;
        resume();
        return false;
    }
    publisher_.setSession(session_);
    external_ = false;
    demandSourceMask_ = 0;
    lastRequestQpc_ = lastSequence_ = lastFirstFrame_ = 0;
    faulted_ = false;
    sourceControlDirty_ = true;
    publishSourceControl();
    if (!QDir().mkpath(path(stateRoot_)))
    {
        faulted_ = true;
        error = "Cannot create private profile";
        resume();
        return false;
    }
    QJsonObject discovery{
        {"abi", "0x00020000"},
        {"owner", "LOCAL"},
        {"publication_endpoint", QString::fromStdWString(publicationEndpoint_)},
        {"session_header_base64",
         QString::fromLatin1(
             QByteArray(reinterpret_cast<const char *>(&session_), sizeof(session_)).toBase64())},
        {"source_control_revision", QString::number(revision_.load())},
        {"manual_mute_mask", static_cast<int>(manualMute_.load())}};
    QSaveFile file(path(stateRoot_ / "StageBusSession.json"));
    file.setDirectWriteFallback(false);
    auto data = QJsonDocument(discovery).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
    {
        faulted_ = true;
        error = "Session discovery write failed";
        resume();
        return false;
    }
    resume();
    return true;
}
bool SourceController::beginCapture(std::string &error)
{
    if (offline_ || baselineOwnsSources())
    {
        error = offline_ ? "OFFLINE — capture disabled" : "WAITING — R10 engine owns audio sources";
        return false;
    }
    if (!pause())
    {
        error = "Source capture prepare quiesce timeout";
        return false;
    }
    bool ok = domains_.prepareMusic(nullptr, graph_, &graph_, saved_.engines[0], saved_.engines[1],
                                    saved_.engines[2]) &&
              domains_.startMusic();
    capturing_ = ok;
    if (!ok)
    {
        resume();
        error = "Source domain prepare/start failed; inputs remain unavailable";
        return false;
    }
    selector_ = std::make_unique<SourceSelectionController>(domains_, modules_);
    selector_->catalog(catalog_);
    selector_->engine = saved_.engines[0];
    selector_->leftChannel = saved_.leftChannel;
    selector_->rightChannel = saved_.rightChannel;
    ok = selector_->selectMusic(saved_.current, error);
    localDiscontinuity_.fetch_or(7);
    resume();
    return ok;
}
void SourceController::service(const SourceCommand &cmd)
{
    std::string error;
    bool ok = true;
    if(cmd.type==SourceCommand::BusMode) { reason_="Stage1 uses continuous local cadence; V1 External demand is incompatible"; return; }
    const bool identityChange =
        cmd.type == SourceCommand::Load || cmd.type == SourceCommand::Select ||
        cmd.type == SourceCommand::BusMode || cmd.type == SourceCommand::Retry;
    struct ResumeGuard
    {
        SourceController *self;
        ~ResumeGuard()
        {
            if (self->controlQuiesce_)
            {
                self->controlQuiesce_ = false;
                self->resume();
            }
        }
    } guard{this};
    if (identityChange)
    {
        if (!pause()) { reason_ = "Source worker could not quiesce"; return; }
        controlQuiesce_ = true;
    }
    if (cmd.type == SourceCommand::Mute)
    {
        if (!pause())
        {
            reason_ = "Mute quiesce timeout";
            return;
        }
        manualMute_ = cmd.value & 7;
        ++revision_;
        saved_.manualMuteMask = manualMute_;
        sourceControlDirty_ = true;
        resume();
        return;
    }
    if (cmd.type == SourceCommand::Save)
        ok = state_->save(coherentState(), error);
    else if (cmd.type == SourceCommand::Import)
    {
        ok = state_->importR10(cmd.project, cmd.ini, error);
        if (ok) ok = state_->save(coherentState(), error);
    }
    else if (cmd.type == SourceCommand::Load)
    {
        SourceStateSnapshot next;
        ok = state_->load(next, error);
        if (ok)
        {
            domains_.stop();
            capturing_ = false;
            saved_ = next;
            saved_.engines = {0, 0, 2};
            manualMute_ = next.manualMuteMask;
            revision_ = (std::max)(revision_.load() + 1, next.revision);
            localDiscontinuity_.fetch_or(7);
            if (!offline_) ok = beginCapture(error);
        }
    }
    else if (cmd.type == SourceCommand::Select)
    {
        if (offline_ || !selector_)
        {
            ok = false;
            error = "MUSIC selection unavailable while capture is offline";
        }
        else
        {
            selector_->catalog(catalog_);
            if (cmd.role == cmd.value)
            {
                ok = false;
                error = "MUSIC channels must be distinct";
            }
            else
            {
                selector_->leftChannel = cmd.role;
                selector_->rightChannel = cmd.value;
                auto previousRevision = selector_->revision();
                const auto previousLeft = saved_.leftChannel, previousRight = saved_.rightChannel;
                ok = selector_->selectMusic(cmd.key, error);
                if (selector_->revision() != previousRevision)
                {
                    saved_.current = cmd.key;
                    saved_.leftChannel = cmd.role;
                    saved_.rightChannel = cmd.value;
                    ++revision_;
                    localDiscontinuity_.fetch_or(1);
                }
                else
                {
                    selector_->leftChannel = previousLeft;
                    selector_->rightChannel = previousRight;
                }
            }
        }
    }
    else if (cmd.type == SourceCommand::Engine)
    {
        ok = cmd.role < 3 && (cmd.role==2 ? (cmd.value==0 || cmd.value==2) : cmd.value==0);
        if (ok) {
            if(cmd.role==2 && saved_.engines[2]!=cmd.value && capturing_) {
                const auto rate=domains_.cinemaTelemetry().source_rate;
                if(cmd.value==0 && rate && !CinemaSourceDomainStream::supportsReferenceRate(rate)) {
                    ok=false;
                    error="CINEMA r8brain comparison supports 44.1/48/88.2/96/176.4/192 kHz; active WDL retained";
                } else ok=domains_.restartCinema(cmd.value);
                if(ok) localDiscontinuity_.fetch_or(4);
                else if(error.empty()) error="CINEMA SRC restart failed";
            }
            if(ok) { saved_.engines[cmd.role]=cmd.value; ++revision_; }
        } else error = cmd.role==2 ? "CINEMA supports WDL or r8brain" : "Stage 1 uses fixed r8brain SRC";
    }
    else if (cmd.type == SourceCommand::Retry)
    {
        if (faulted_)
            ok = newSession(false, error);
        if (ok && !capturing_)
            ok = beginCapture(error);
        else if (ok && selector_)
        {
            selector_->catalog(catalog_);
            auto previous = selector_->revision();
            ok = selector_->selectMusic(saved_.current, error);
            if (selector_->revision() != previous) { ++revision_; localDiscontinuity_.fetch_or(1); }
        }
    }
    reason_ = ok ? "Source command applied" : error;
}
void SourceController::controlWorker()
{
    try
    {
        InputSourceCatalog discovery;
        auto retry = std::chrono::steady_clock::now();
        auto publicationService = retry;
        while (!stopping_)
        {
            if (std::chrono::steady_clock::now() >= retry)
            {
                auto fresh = discovery.refresh(modules_);
                {
                    std::lock_guard lock(mutex_);
                    catalog_ = std::move(fresh);
                    if (!capturing_)
                    {
                        std::string error;
                        if (!beginCapture(error))
                            reason_ = error;
                        else
                            reason_ = "Source capture connected; MUSIC Transport selected";
                    }
                    else if (selector_ && 
                             SourceSelectionController::needsRetry(domains_.musicPrepared(),
                                                                   domains_.musicTelemetry()))
                    {
                        selector_->catalog(catalog_);
                        std::string error;
                        if (pause())
                        {
                            auto previous = selector_->revision();
                            if (!selector_->selectMusic(saved_.current, error)) reason_ = error;
                            if (selector_->revision() != previous) { ++revision_; sourceControlDirty_ = true; localDiscontinuity_.fetch_or(1); }
                            resume();
                        }
                    }
                }
                retry = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
            {
                std::lock_guard lock(mutex_);
                if (!commands_.empty())
                {
                    auto cmd = std::move(commands_.front());
                    commands_.pop_front();
                    service(cmd);
                    sourceControlDirty_ = true;
                }
            }
            {
                std::lock_guard lock(mutex_);
                serviceStageLink();
                publishSourceControl();
            }
            domains_.reapRetiredMusic();
            const auto now=std::chrono::steady_clock::now();
            if(publisher_.needsService() || now>=publicationService)
            {
                std::lock_guard lock(mutex_);
                servicePublication();
                publicationService=now+std::chrono::seconds(1);
            }
            Sleep(10);
        }
    }
    catch (const std::exception &e)
    {
        std::lock_guard lock(mutex_);
        reason_ = e.what();
        ++faults_;
    }
}
void SourceController::audioWorker() noexcept
{
    ProAudioThreadScope realtime;
    SourceCycleTimer timer;
    if (!timer.valid())
    {
        faulted_ = true;
        ++faults_;
        return;
    }
    auto epoch = std::chrono::steady_clock::now();
    auto scheduled = epoch;
    auto blockBegin = epoch;
    uint64_t cycles = 0;
    StageBusSessionHeader owned = session_;
    entropy::stage::SourceFrameDemand demand(owned);
    uint64_t previousGeneration[3]{};
    bool hasRequest = false;
    StageBusFrameRequest request{};
    while (!stopping_)
    {
        if (pause_)
        {
            const bool preserveCadence=publicationPause_.load();
            paused_ = true;
            while (pause_ && !stopping_)
                timer.waitUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(200));
            if (std::memcmp(owned.session_id, session_.session_id, 16) != 0)
            {
                owned = session_;
                demand = entropy::stage::SourceFrameDemand(owned);
                cycles = 0;
                epoch = std::chrono::steady_clock::now();
                hasRequest = false;
                std::fill(std::begin(previousGeneration), std::end(previousGeneration), 0);
            }
            else if(!preserveCadence)
                epoch = EngineCadencePolicy::rebasedEpoch(std::chrono::steady_clock::now(), cycles);
            if(!preserveCadence) scheduled = std::chrono::steady_clock::now();
            // Keep control waiting until all non-atomic session reads finish.
            paused_ = false;
            continue;
        }
        if (faulted_)
        {
            timer.waitUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(200));
            continue;
        }
        if (!hasRequest)
        {
            if (std::chrono::steady_clock::now() < scheduled)
            {
                timer.waitUntil(std::chrono::steady_clock::now() + std::chrono::microseconds(200));
                continue;
            }
            if (!harness_.next(revision_, request) ||
                demand.accept(request) != entropy::stage::DemandResult::Accepted)
            {
                faulted_ = true;
                ++faults_;
                continue;
            }
            hasRequest = true;
        }
        blockBegin = std::chrono::steady_clock::now();
        demandSourceMask_ = 7;
        const auto sources =
            pollProgramSources(domains_, capturing_.load(), false, 3);
        const SourceBlock1024 *blocks[3] = {&sources.music, &sources.aux, &sources.cinema};
        bool available[3] = {sources.musicAvailable, sources.auxAvailable, sources.cinemaAvailable};
        std::array<SourceDomainTelemetry, 3> telemetry = {
            domains_.musicTelemetry(), domains_.auxTelemetry(), domains_.cinemaTelemetry()};
        AudioBlockReport reports[3]{};
        for (unsigned r = 0; r < 3; ++r)
        {
            auto &report = reports[r];
            report.passed = available[r];
            report.convertedAudioPointers = blocks[r]->channel;
            report.convertedAudioPointerFrames = available[r] ? 1024 : 0;
            report.streamGeneration =
                available[r] ? blocks[r]->generation : telemetry[r].generation;
            report.state = available[r] ? 2 : telemetry[r].state;
            report.sourceRate =
                available[r] ? blocks[r]->descriptor.source_rate : telemetry[r].source_rate;
        }
        const auto localMute = manualMute_.load();
        const entropy::stage::SourceFrameEnvelope delivery(request);
        auto result =
            executor_.process(delivery, localMute, reports[0], reports[1], reports[2], outPointers_.data());
        result.header.discontinuity_mask |= localDiscontinuity_.exchange(0);
        result.header.qpc_timestamp = qpc();
        result.header.music_session_revision = sources.music.sessionRevision;
        for (unsigned r = 0; r < 3; ++r)
        {
            if (telemetry[r].discontinuity_flags ||
                previousGeneration[r] != result.header.source_generation[r])
                result.header.discontinuity_mask |= 1u << r;
            previousGeneration[r] = result.header.source_generation[r];
        }
        if (!result.passed)
        {
            faulted_ = true;
            ++faults_;
            continue;
        }
        publisher_.publish(result.header, sendPointers_.data());
        lastSequence_ = result.header.block_sequence;
        lastFirstFrame_ = result.header.first_frame;
        for (unsigned c = 0; c < 10; ++c)
        {
            double peak = 0;
            for (double sample : output_[c])
                peak = (std::max)(peak, std::abs(sample));
            outputPeaks_[c] = peak;
        }
        appliedManualMute_ = localMute;
        lastValidity_ = result.header.validity_mask;
        ++blocks_;
        ++cycles;
        scheduled = EngineCadencePolicy::boundedDeadline(
            epoch + std::chrono::nanoseconds(EngineCadencePolicy::deadlineNanoseconds(0, cycles)),
            std::chrono::steady_clock::now(),
            std::chrono::nanoseconds(1024ull * 1000000000 / 192000), blockBegin);
        if (!demand.complete(request.block_sequence) || !harness_.complete(request.block_sequence))
        {
            faulted_ = true;
            ++faults_;
        }
        hasRequest = false;
    }
    paused_ = false;
}
} // namespace entropy::host

namespace entropy::host
{
void SourceController::publishSourceControl()
{
    // Local control_revision and source-generation metadata travel in each
    // completed V2 block. No consumer-owned control queue participates.
    sourceControlDirty_ = false;
}
} // namespace entropy::host
