// Offline-only integration access. Missing graph taps must fail compilation
// before adding read access; no processing or DSP behavior is changed.
#include "Engine/Control/GraphControlAccess.h"
int main(){entropy::engine::AudioGraph g;return entropy::engine::GraphControlAccess::crossoverOutput(g).capacity()!=0;}
