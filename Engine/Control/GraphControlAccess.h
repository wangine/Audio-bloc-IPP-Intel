#pragma once
#include "Engine/AudioGraph.h"
namespace entropy::engine {
// Control ownership only. No reference counting in the processing callback.
class GraphControlAccess {
public:
 static auto crossover(AudioGraph& g){return g.crossover_;}
 static auto& matrix(AudioGraph& g){return g.matrix_;}
 static auto& headroom(AudioGraph& g){return g.headroom_;}
 static auto maximumFrames(const AudioGraph& g){return g.maximumFrames_;}
 static void invalidateArm(AudioGraph& g){g.egressPolicyReady_=false;}
};
}
