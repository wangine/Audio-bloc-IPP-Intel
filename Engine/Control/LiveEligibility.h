#pragma once
#include "ControlPlane.h"
#include <filesystem>
namespace entropy::control {
bool liveHeadroomReady(engine::AudioGraph&,bool requireSettled=true) noexcept;
bool liveCrossoverChangeAllowed(const ipc::Value&,const ipc::Value& request);
bool liveFirAllowed(const ipc::Value& crossoverSnapshot);
bool liveGateMatches(const std::filesystem::path&,const std::string& executableSha256);
// Copy/import the current profile into an independent offline graph. Never
// advances the live histories, changes coefficients, or opens a device.
ipc::Value liveProfilePreflight(ControlPlane&);
// Owned by the serialized control thread. Only a successful strict validation
// of the exact requested configuration may be reused; never grants ARM itself.
class LiveProfilePreflightCache {
public:
 ipc::Value run(ControlPlane&);
 void invalidate() noexcept;
private:
 std::string configuration_;
 std::vector<unsigned char> crossoverState_;
 ipc::Value report_;
 uint64_t validationRun_=0;
};
}
