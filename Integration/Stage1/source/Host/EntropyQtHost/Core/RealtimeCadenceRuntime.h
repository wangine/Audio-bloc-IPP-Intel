#pragma once

#include "Core/ProAudioThreadScope.h"
#include "Core/RealtimeControlQueue.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace entropy::host
{
using RealtimeTickFn = bool (*)(void*, std::uint64_t streamFrame) noexcept;
using RealtimeControlApplyFn = void (*)(
    void*, const RealtimeControlCommand&) noexcept;

struct RealtimeClockApi final
{
    void* context = nullptr;
    std::uint64_t (*nowNanoseconds)(void*) noexcept = nullptr;
    bool (*waitUntilNanoseconds)(void*, std::uint64_t) noexcept = nullptr;
};

struct RealtimeCadenceDependencies final
{
    RealtimeClockApi clock{};
    ProAudioApi proAudio{};
    RealtimeControlQueue* controls = nullptr;
    RealtimeControlApplyFn applyControl = nullptr;
    void* controlContext = nullptr;
};

enum class RealtimeCadenceState : std::uint32_t
{
    stopped = 0,
    starting = 1,
    running = 2,
    stopping = 3,
    faulted = 4
};

struct RealtimeCadenceTelemetry final
{
    std::uint32_t state = 0;
    std::uint32_t mmcssActive = 0;
    std::uint64_t producedBlocks = 0;
    std::uint64_t producedFrames = 0;
    std::uint64_t lateWakeups = 0;
    std::uint64_t overBudgetBlocks = 0;
    std::uint64_t rejectedCommands = 0;
    std::uint64_t catchupBlocks = 0;
    std::uint64_t droppedBlocks = 0;
    double lastProcessUs = 0.0;
    double maximumProcessUs = 0.0;
    double maximumWakeLatenessUs = 0.0;
};

class RealtimeCadenceRuntime final
{
public:
    explicit RealtimeCadenceRuntime(
        RealtimeCadenceDependencies dependencies = {}) noexcept;
    ~RealtimeCadenceRuntime();

    RealtimeCadenceRuntime(const RealtimeCadenceRuntime&) = delete;
    RealtimeCadenceRuntime& operator=(const RealtimeCadenceRuntime&) = delete;

    bool start(RealtimeTickFn tick, void* tickContext) noexcept;
    void requestStop() noexcept;
    void join() noexcept;
    RealtimeCadenceTelemetry snapshot() const noexcept;
    const char* failureReason() const noexcept;

private:
    void workerMain() noexcept;
    std::uint64_t nowNanoseconds() const noexcept;
    bool waitUntilNanoseconds(std::uint64_t deadline) noexcept;
    void publishMaximum(std::atomic<double>& target, double value) noexcept;

    RealtimeCadenceDependencies dependencies_{};
    RealtimeTickFn tick_ = nullptr;
    void* tickContext_ = nullptr;
    void* waitableTimer_ = nullptr;
    std::thread worker_{};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::uint32_t> startupResult_{0};
    std::atomic<std::uint32_t> state_{
        static_cast<std::uint32_t>(RealtimeCadenceState::stopped)};
    std::atomic<std::uint32_t> mmcssActive_{0};
    std::atomic<std::uint64_t> producedBlocks_{0};
    std::atomic<std::uint64_t> producedFrames_{0};
    std::atomic<std::uint64_t> lateWakeups_{0};
    std::atomic<std::uint64_t> overBudgetBlocks_{0};
    std::atomic<std::uint64_t> rejectedCommands_{0};
    std::atomic<double> lastProcessUs_{0.0};
    std::atomic<double> maximumProcessUs_{0.0};
    std::atomic<double> maximumWakeLatenessUs_{0.0};
    std::atomic<const char*> failureReason_{""};
};
}
