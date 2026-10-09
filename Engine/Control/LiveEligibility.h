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
}
