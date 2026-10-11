#pragma once
#include <entropy/center_abi.h>
#include <array>
#include <filesystem>
#include <string>
#include <windows.h>
namespace entropy::stage {
inline constexpr std::array<const char*,16> centerIds={"mode","fftSize","threshold","selectivity","floor","attackMs","releaseMs","smoothingHz","midWeight","levelWeight","phaseWeight","coherenceWeight","detectorMode","lookAheadMs","centerGain","centerEnable"};
inline constexpr std::array<double,16> centerMin={0,0,0,.25,0,1,10,0,0,0,0,0,0,0,-24,0};
inline constexpr std::array<double,16> centerMax={1,4,.95,8,.5,200,600,600,2,2,2,2,4,50,12,1};
struct CenterSettings {std::array<double,16> values={1,2,.45,2,.08,20,140,50,1,1,1,1,0,0,0,1};};
bool validCenterSettings(const CenterSettings&) noexcept;
struct alignas(8) CenterSnapshot {uint64_t loaded=0,latencyValid=0,revision=0;CenterSettings settings{};EntropyCenterTelemetry telemetry{};};
class DspCenterModule {
public:
 ~DspCenterModule(){close();}
 DspCenterModule()=default;
 DspCenterModule(const DspCenterModule&)=delete;
 bool prepare(const std::filesystem::path&,std::string&);
 bool set(unsigned,double) noexcept;
 bool apply(const CenterSettings&,uint64_t revision) noexcept;
 bool reset() noexcept;
 bool process(const double*,const double*,double*) noexcept;
 void close() noexcept;
 bool loaded() const noexcept {return handle_!=nullptr;}
 CenterSnapshot snapshot() const noexcept {return snapshot_;}
private:
 HMODULE library_=nullptr;EntropyModuleHandle handle_=nullptr;
 EntropyModuleApiV1 api_{};EntropyCenterApiV1 center_{};EntropyCenterTelemetryApiV1 telemetry_{};
 CenterSnapshot snapshot_{};bool configured_=false;
};
}
