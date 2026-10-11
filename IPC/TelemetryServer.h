#pragma once
#include "Engine/AudioScheduler.h"
namespace entropy::ipc {
Value telemetrySnapshot(const control::ControlPlane&,const control::AudioScheduler&,bool offlineSignal,const input::Stage2Ingress* stage2=nullptr);
Value programTelemetrySnapshot(const control::ControlPlane&,const control::AudioScheduler&,const input::ProgramIngress&,const input::ProgramSelection&,const std::wstring& music,const std::wstring& cinema);
}
