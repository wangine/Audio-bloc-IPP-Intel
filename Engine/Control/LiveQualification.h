#pragma once
#include "LiveEligibility.h"
namespace entropy::control {
// Linked ONLY into EntropyLiveQualification, never EntropyAudioCore.
ipc::Value liveProfilePreflight(ControlPlane&,const ipc::Value& change=ipc::Value{});
}
