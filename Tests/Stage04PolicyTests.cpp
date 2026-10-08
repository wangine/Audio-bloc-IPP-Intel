#include "Engine/AudioGraph.h"
#include <cstdio>
template<class T> int test() {
    if constexpr (requires(T& g, const EntropyHeadroomState& s) {
        g.prepareForEgress(256, s, 20.0); g.egressPolicyReady();
    }) {
        T graph; EntropyHeadroomState invalid{};
        if(graph.prepareForEgress(256, invalid, 20.0)) return 2;
        if(!graph.prepare(256) || graph.egressPolicyReady()) return 3;
        std::puts("PASS explicit egress policy rejects missing ledger and offline unity"); return 0;
    } else { std::puts("FAIL missing explicit egress headroom policy"); return 1; }
}
int main(){return test<entropy::engine::AudioGraph>();}
