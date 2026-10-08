#pragma once
#include "Engine/AudioScheduler.h"
namespace entropy::ipc {
Value telemetrySnapshot(const control::ControlPlane&,const control::AudioScheduler&,bool offlineSignal);
}
