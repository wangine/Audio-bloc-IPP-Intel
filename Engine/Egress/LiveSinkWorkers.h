#pragma once
#include "EgressContract.h"
#include "IPC/Json.h"
#include <array>
#include <functional>
#include <memory>
#include <string>
namespace entropy::control {
struct LiveDomain {
 bool enabled=false;unsigned channels=0,format=0;std::string endpoint,logicalId;
 std::array<unsigned,16> lanes{};
 // Ephemeral preflight result, never written back into the saved profile.
 bool liveAllowed=true;std::string liveError;
};
class LiveSinkWorkers final {
public:
 using Factory=std::function<std::unique_ptr<egress_host::Sink>(unsigned)>;
 explicit LiveSinkWorkers(Factory={},bool legacyCalibrationArgument=true);
 ~LiveSinkWorkers();
 bool prepare(const std::array<LiveDomain,2>&);
 bool start();
 void stop() noexcept;
 void closePublicationGate() noexcept;
 void atBlockBoundary() noexcept;
 void graphStopped() noexcept;
 void publish(const double* const*,unsigned,bool) noexcept;
 void publish(const double* const*,unsigned,bool,egress_host::GraphInterval) noexcept;
 void failAll(unsigned) noexcept;
 void waitForInput() noexcept;
 bool sourceWaitFault() const noexcept;
 unsigned sourceWaitFaults() const noexcept;
 bool armed() const noexcept;
 bool recoveryAtTarget() const noexcept;
 bool faulted() const noexcept;
 bool engaged() const noexcept;
 ipc::Value snapshot() const;
 static std::array<LiveDomain,2> decode(const ipc::Value&);
 static uint32_t renderLaneMask(const std::array<LiveDomain,2>&) noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
