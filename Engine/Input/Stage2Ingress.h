#pragma once
#include <StageBusWin32.h>
#include <array>
#include <atomic>
#include <thread>

namespace entropy::input {
enum class State : unsigned { Disabled, Waiting, Receiving, Fault, OwnershipBusy };
struct Telemetry {
    State state=State::Disabled;
    uint64_t received=0, consumed=0, empty=0, faults=0, generation=0;
    unsigned queued=0, producerPid=0, validityMask=0;
    uint64_t lastBlockQpc=0;
};
class Stage2Ingress {
public:
    static constexpr unsigned frames=1024, channels=11, capacity=4;
    Stage2Ingress()=default;
    ~Stage2Ingress(){stop();}
    bool start(std::wstring endpoint={}, bool claimRenderOwner=true);
    void stop() noexcept;
    bool tryRead(double* const* output13) noexcept;
    Telemetry telemetry() const noexcept;
    static const char* stateText(State) noexcept;
private:
    struct Slot { uint64_t generation=0; StageBusBlockHeader header{};
        std::array<std::array<double,frames>,channels> planes{}; };
    void run(std::wstring,bool) noexcept;
    std::array<Slot,capacity> queue_{};
    alignas(64) std::atomic<uint64_t> written_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
    std::atomic<uint64_t> generation_{0},received_{0},consumed_{0},empty_{0},faults_{0};
    std::atomic<unsigned> producerPid_{0},validityMask_{0};
    std::atomic<uint64_t> lastBlockQpc_{0};
    std::atomic<State> state_{State::Disabled};
    std::atomic<bool> stopping_{false};
    std::thread worker_;
};
}
