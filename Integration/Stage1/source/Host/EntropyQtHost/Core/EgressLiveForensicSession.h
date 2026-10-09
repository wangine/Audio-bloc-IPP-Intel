#pragma once

#include "EgressForensicCollector.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace entropy::host
{
struct EgressLiveSourceState
{
    std::uint64_t tidalGeneration = 0;
    std::uint64_t auxGeneration = 0;
    std::uint64_t cinemaGeneration = 0;
    std::uint32_t tidalRate = 0;
    std::uint32_t auxRate = 0;
    std::uint32_t cinemaRate = 0;
};

struct EgressLiveHostFaultContext
{
    std::uint64_t egressTimingCallbacks = 0;
    std::uint64_t egressDeadlineMisses = 0;
    std::uint64_t lastThreadCycles = 0;
    std::uint64_t maxThreadCycles = 0;
    double callbackIntervalUs = 0.0;
    double maxCallbackIntervalUs = 0.0;
    double lastClockUs = 0.0;
    double lastGuardianUs = 0.0;
    double lastPcmUs = 0.0;
    double lastSubmitUs = 0.0;
    double lastTotalUs = 0.0;
    double maxClockUs = 0.0;
    double maxGuardianUs = 0.0;
    double maxPcmUs = 0.0;
    double maxSubmitUs = 0.0;
    double maxTotalUs = 0.0;
};

class EgressLiveForensicSession final
{
public:
    EgressLiveForensicSession(
        EntropyModuleHandle handle,
        EntropyEgressDiagnosticsApiV1 compact,
        EntropyEgressForensicApiV1 trace,
        std::filesystem::path root,
        std::map<std::string, std::string> staticMetadata);
    ~EgressLiveForensicSession();

    EgressLiveForensicSession(const EgressLiveForensicSession&) = delete;
    EgressLiveForensicSession& operator=(const EgressLiveForensicSession&) = delete;

    bool start(std::string& error);
    void stop() noexcept;
    void updateSourceState(const EgressLiveSourceState& state) noexcept;
    void notifyHostFault(const char* detail, const char* failedStage,
                         double failedStageUs,
                         const EgressLiveHostFaultContext& context) noexcept;

    bool exported() const noexcept { return exported_.load(std::memory_order_acquire); }
    std::filesystem::path exportedPath() const;
    std::string lastError() const;

private:
    void run() noexcept;
    std::map<std::string, std::string> metadataSnapshot() const;
    std::filesystem::path makeDestination() const;
    bool exportHostOnly(const std::filesystem::path& destination,
                        const std::map<std::string, std::string>& metadata,
                        std::string& error) const noexcept;
    void setError(const std::string& value) noexcept;

    EntropyModuleHandle handle_{};
    EntropyEgressDiagnosticsApiV1 compact_{};
    EntropyEgressForensicApiV1 trace_{};
    std::filesystem::path root_;
    std::map<std::string, std::string> staticMetadata_;
    std::unique_ptr<EgressForensicCollector> collector_;
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> exported_{false};

    std::atomic<std::uint64_t> tidalGeneration_{0}, auxGeneration_{0}, cinemaGeneration_{0};
    std::atomic<std::uint32_t> tidalRate_{0}, auxRate_{0}, cinemaRate_{0};

    mutable std::mutex stateMutex_;
    bool hostFaulted_ = false;
    std::string hostFault_;
    std::string failedStage_;
    double failedStageUs_ = 0.0;
    EgressLiveHostFaultContext faultContext_{};
    std::filesystem::path exportedPath_;
    std::string lastError_;
};
} // namespace entropy::host
