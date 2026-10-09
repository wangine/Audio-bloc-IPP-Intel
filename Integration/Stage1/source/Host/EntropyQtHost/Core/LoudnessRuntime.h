#pragma once

#include "OptionalAudioStageRuntime.h"

namespace entropy::host
{
// Compatibility name for the Slot 1 control bridge. Processing behavior is
// provided by the generic optional-stage runtime used by every optional DSP.
using LoudnessRuntime = OptionalAudioStageRuntime;
}
