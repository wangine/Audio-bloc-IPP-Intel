#include "Engine/AudioGraph.h"
#include <cstdio>
#include <concepts>
#include <limits>
template<class T> int verify() {
    if constexpr (requires(T& g) {
        { g.setOfflineParameter("musicGainDb1", -6.0) } -> std::same_as<bool>;
        { g.applyOfflineParameters(true) } -> std::same_as<bool>;
    }) {
        T graph;
        if (graph.setOfflineParameter("musicGainDb1", -6.0)) return 2;
        if (!graph.prepare(256)) return 3;
        if (graph.setOfflineParameter("missing", 0)) return 4;
        if (graph.setOfflineParameter("musicGainDb1", std::numeric_limits<double>::quiet_NaN())) return 5;
        if (!graph.setOfflineParameter("musicGainDb1", -6.0) || !graph.applyOfflineParameters(true)) return 6;
        std::puts("PASS offline configuration validation"); return 0;
    } else { std::puts("FAIL missing offline parameter configuration"); return 1; }
}
int main() { return verify<entropy::engine::AudioGraph>(); }
