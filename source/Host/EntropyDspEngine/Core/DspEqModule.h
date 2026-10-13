#pragma once
#include <entropy/parametric_eq_abi.h>
#include <array>
#include <filesystem>
#include <string>
#include <windows.h>
namespace entropy::stage {
using EqSettings=EntropyParametricEqSettingsV1;
EqSettings defaultEqSettings() noexcept;
bool validEqSettings(const EqSettings&) noexcept;
struct EqSchema {std::array<EntropyParametricEqParameterV1,121> parameters{};char version[32]{};};
struct alignas(8) EqSnapshot {
 uint64_t loaded=0,latencyValid=0,latencySamples=0,revision=0,appliedRevision=0,nativeSequence=0;
 EqSettings settings{};EntropyParametricEqTelemetryV1 telemetry{};
};
class DspEqModule {
public:
 DspEqModule()=default;
 DspEqModule(const DspEqModule&)=delete;
 DspEqModule& operator=(const DspEqModule&)=delete;
 ~DspEqModule(){close();}
 bool prepare(const std::filesystem::path&,std::string&);
 bool apply(const EqSettings&,uint64_t) noexcept;
 bool process(const double*,const double*,double*,double*) noexcept;
 bool readControlSnapshot() noexcept; // worker control phase, outside audio block execution
 bool reset() noexcept;
 void close() noexcept;
 bool loaded() const noexcept{return handle_!=nullptr;}
 bool pending() const noexcept{return snapshot_.nativeSequence!=snapshot_.telemetry.applied_sequence;}
 EqSnapshot snapshot() const noexcept{return snapshot_;}
 const EqSchema& schema() const noexcept{return schema_;}
private:
 HMODULE library_=nullptr;EntropyModuleHandle handle_=nullptr;
 EntropyModuleApiV1 api_{};EntropyParametricEqApiV1 eq_{};EqSchema schema_{};EqSnapshot snapshot_{};
 bool controlDirty_=false;
};
}
