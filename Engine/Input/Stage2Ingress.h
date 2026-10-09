#pragma once
#include "Transport/StagePublishV2/StagePublishV2.h"
#include <array>
#include <atomic>
#include <thread>
namespace entropy::input {
enum class State : unsigned { Disabled, Waiting, Receiving, Fault, OwnershipBusy };
struct Telemetry {
    State state=State::Disabled;
    uint64_t received=0, consumed=0, empty=0, faults=0, generation=0;
    unsigned queued=0, producerPid=0, validityMask=0;
    unsigned sourceDiscontinuityMask=0;
    uint64_t lastBlockQpc=0, releaseFailures=0;
    uint64_t gaps=0, lastSequence=0, lastFirstFrame=0, lastEpoch=0, sourceQpc=0, sourceFlaggedBlocks=0;
    std::array<uint8_t,16> sourceSession{};
};
// Only the graph owner reads this consumer's V2 ring. No source requests or leases.
class Stage2Ingress {
public:
    static constexpr unsigned frames=1024, channels=11;
    ~Stage2Ingress(){stop();}
    bool start(std::wstring endpoint={}, bool claimRenderOwner=true);
    void stop() noexcept;
    void restartSubscription() noexcept;
    bool tryRead(double* const* output13) noexcept;
    Telemetry telemetry() const noexcept;
    StageBusBlockHeader lastBlock() const noexcept {return lastBlock_;} // graph owner only
    static const char* stateText(State) noexcept;
private:
    void run(std::wstring,bool) noexcept;
    publish::Subscriber subscriber_;
    // Graph attempts once, without waiting. Only the non-RT connector waits
    // for an active reader to finish before unmapping/reconnecting.
    std::atomic_flag access_=ATOMIC_FLAG_INIT;
    bool connected_=false;
    std::atomic<bool> stopping_{false}, reconnect_{false};
    std::atomic<State> state_{State::Disabled};
    std::atomic<uint64_t> generation_{0}, received_{0}, empty_{0}, faults_{0};
    std::atomic<uint64_t> lastBlockQpc_{0}, gaps_{0}, sequence_{0}, firstFrame_{0}, epoch_{0}, sourceQpc_{0};
    std::array<std::atomic<uint8_t>,16> sourceSession_{};
    std::atomic<unsigned> producerPid_{0}, validityMask_{0};
    std::atomic<unsigned> sourceDiscontinuityMask_{0};
    std::atomic<uint64_t> sourceFlaggedBlocks_{0};
    StageBusBlockHeader lastBlock_{};
    std::thread worker_;
};
}
