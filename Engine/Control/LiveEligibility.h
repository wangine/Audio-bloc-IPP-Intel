#pragma once
#include "ControlPlane.h"
#include <filesystem>
namespace entropy::control {
bool liveHeadroomReady(engine::AudioGraph&,bool requireSettled=true) noexcept;
bool liveCrossoverChangeAllowed(const ipc::Value&,const ipc::Value& request);
bool liveSceneChangeRequested(const ipc::Value& request);
bool liveFirAllowed(const ipc::Value& crossoverSnapshot);
bool liveGateMatches(const std::filesystem::path&,const std::string& executableSha256);
uint64_t diagnosticBenchmarkCalls() noexcept;
void recordDiagnosticBenchmarkCall() noexcept; // Only the separately linked diagnostic calls this.
}
