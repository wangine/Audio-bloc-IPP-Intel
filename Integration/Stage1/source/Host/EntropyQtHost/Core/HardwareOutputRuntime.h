#pragma once

#include "EngineTelemetry.h"
#include "EgressProgramBoundary.h"
#include "FirstOutputFault.h"
#include "ModuleRegistry.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace entropy::host
{
struct LiveInputProbeReport;
struct AudioBlockReport;
class CoordinatedDomainStart; class R05DiagnosticCapture;

inline constexpr bool validHardwareOutputDomainCount(
    std::uint32_t count) noexcept
{
    return count >= 1 && count <= engineMaximumDacDomains;
}

class HardwareOutputRuntime final
{
public:
    HardwareOutputRuntime();
    void setDiagnosticCapture(R05DiagnosticCapture* capture) noexcept {r05Capture_=capture;}
    ~HardwareOutputRuntime();
    HardwareOutputRuntime(const HardwareOutputRuntime&) = delete;
    HardwareOutputRuntime& operator=(const HardwareOutputRuntime&) = delete;

    bool arm(const ModuleRegistry& registry,
             const EngineDacDomainControl* domains,
             std::uint32_t domainCount,
             std::string& error);
    void disarm() noexcept;
    void push(const LiveInputProbeReport& report) noexcept;
    void push(const AudioBlockReport& report) noexcept;
    bool observeSourceState(std::uint64_t musicSessionRevision,
                            std::uint64_t tidalGeneration,
                            std::uint64_t auxGeneration,
                            std::uint64_t cinemaGeneration,
                            std::uint32_t tidalRate,
                            std::uint32_t auxRate,
                            std::uint32_t cinemaRate,
                            bool cinemaProgram, std::uint64_t programRevision = 0,
                            std::uint64_t programSession = 0, std::uint64_t producerRevision = 0) noexcept;
    void telemetry(EngineTelemetrySnapshot& snapshot) const noexcept;
    bool armed() const noexcept;
    bool faulted() const noexcept;
    bool recoveryAtTarget() const noexcept;

private:
    R05DiagnosticCapture* r05Capture_=nullptr;
    class Domain;
    FirstOutputFault firstFault_;
    EgressProgramBoundary programBoundary_;
    std::array<std::unique_ptr<Domain>, engineMaximumDacDomains> domains_{};
    std::unique_ptr<CoordinatedDomainStart> startBarrier_;
    std::uint32_t domainCount_ = 0;
    std::string detail_ = "Output gate CLOSED — no ARM request";
};
} // namespace entropy::host
