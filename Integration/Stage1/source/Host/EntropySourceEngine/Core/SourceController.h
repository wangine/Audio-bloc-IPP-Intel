#pragma once
#include "EngineTelemetry.h"
#include "StageLinkServer.h"
#include "SourceBlockExecutor.h"
#include "SourceDemandHarness.h"
#include "SourceSelectionController.h"
#include "SourceState.h"
#include "StageBusWin32.h"
#include "StagePublishV2.h"
#include <deque>
#include <mutex>
#include <thread>
namespace entropy::host
{
struct SourceCommand
{
    enum Type
    {
        Select,
        Engine,
        Save,
        Load,
        Import,
        BusMode,
        Retry,
        Mute
    } type = Retry;
    InputSourceKey key;
    uint32_t role = 0, value = 0;
    std::filesystem::path project, ini;
};
struct SourceUiSnapshot
{
    std::string reason;
    InputSourceSnapshot catalog;
    SourceStateSnapshot state;
    uint64_t blocks = 0, faults = 0, duplicates = 0;
    uint64_t publishDrops = 0;
    bool capturing = false, external = false, sourceControlPending = false;
    uint64_t sequence = 0, firstFrame = 0;
    std::array<SourceDomainTelemetry, 3> sources{};
    StageBusSessionHeader session{};
    std::array<double, 10> outputPeaks{};
    uint32_t outputValidity = 0;
    uint32_t demandSourceMask = 0;
};
class SourceController
{
  public:
    ~SourceController() { stop(); }
    bool start(const std::filesystem::path &moduleRoot, const std::filesystem::path &stateRoot,
               std::string &error, bool offline = false);
    void stop() noexcept;
    bool command(SourceCommand);
    bool enableStageLink(std::wstring endpoint,std::string& error);
    bool enablePublication(std::wstring endpoint,std::string& error);
    void mute(uint32_t mask) noexcept;
    SourceUiSnapshot snapshot() const;
    bool telemetry(EngineTelemetrySnapshot &) const;
    static bool verifyModules(const std::filesystem::path &, std::string &);
    static bool baselineOwnsSources();
    uint64_t blocks() const noexcept { return blocks_; }

  private:
    friend struct SourceLinkRaceProbe;
    void (*leaseGrantedProbe_)(void*)=nullptr;
    void (*requestAdoptedProbe_)(void*)=nullptr;
    void* linkProbeContext_=nullptr;
    void controlWorker();
    void serviceStageLink();
    void servicePublication();
    void audioWorker() noexcept;
    bool pause();
    void resume() noexcept;
    bool newSession(bool external, std::string &);
    bool beginCapture(std::string &);
    void service(const SourceCommand &);
    void publishSourceControl();
    SourceStateSnapshot coherentState() const;
    std::filesystem::path modules_, stateRoot_;
    SourceGraphRuntime graph_;
    SourceDomainRuntime domains_;
    SourceBlockExecutor executor_;
    std::unique_ptr<SourceSelectionController> selector_;
    std::unique_ptr<SourceState> state_;
    SourceStateSnapshot saved_{};
    InputSourceSnapshot catalog_;
    std::array<EngineSrcEngineCapability, engineMaximumSrcEngines> capabilities_{};
    uint32_t capabilityCount_ = 0;
    mutable std::mutex mutex_;
    std::deque<SourceCommand> commands_;
    std::string reason_;
    StageBusSessionHeader session_{};
    SourceDemandHarness harness_;
    entropy::publish::Publisher publisher_;
    std::wstring publicationEndpoint_;
    bool publicationStarted_ = false;
    std::atomic<bool> publicationPause_{false};
    std::unique_ptr<entropy::link::StageLinkServer> stageLink_;
    void *ownerMutex_ = nullptr;
    std::thread control_, audio_;
    std::atomic<bool> stopping_{false}, pause_{false}, paused_{false}, capturing_{false},
        external_{false}, faulted_{false};
    std::atomic<uint32_t> manualMute_{0};
    std::atomic<uint64_t> revision_{1}, blocks_{0}, faults_{0}, duplicates_{0}, lastValidity_{0};
    bool offline_ = false;
    bool controlQuiesce_ = false;
    std::atomic<uint64_t> lastRequestQpc_{0}, lastSequence_{0}, lastFirstFrame_{0};
    bool sourceControlDirty_ = true;
    std::atomic<uint32_t> appliedManualMute_{0};
    std::atomic<uint32_t> demandSourceMask_{0}, localDiscontinuity_{0};
    uint64_t latestMusicRevision_ = 0;
    std::array<std::atomic<double>, 10> outputPeaks_{};
    std::array<std::array<double, 1024>, 10> output_{}, localSink_{};
    std::array<double *, 10> outPointers_{}, sinkPointers_{};
    std::array<const double *, 10> sendPointers_{};
};
} // namespace entropy::host
