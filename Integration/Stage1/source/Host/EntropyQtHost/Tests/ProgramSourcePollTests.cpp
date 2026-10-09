#include "Core/ProgramSourcePoll.h"
#include <array>
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
using namespace entropy::host;
struct Sources {
    std::array<SourceFifo192,3> fifo;
    std::array<std::array<double,1024>,3> in{}, out{};
    bool pop(unsigned i, SourceBlock1024& block) {
        double* output = out[i].data(); std::uint64_t gen = 0;
        if (!fifo[i].pop(&output,1024,gen)) return false;
        block.channels = 1; block.generation = gen; block.channel[0] = output; return true;
    }
    bool popTidal1024(SourceBlock1024& b) { return pop(0,b); }
    bool popAux1024(SourceBlock1024& b) { return pop(1,b); }
    bool popCinema1024(SourceBlock1024& b) { return pop(2,b); }
};
int main() {
    Sources s;
    for (unsigned i=0;i<3;++i) { CHECK(s.fifo[i].prepare(1,4096)); s.fifo[i].reset(1); s.in[i].fill(double(i+1)); }
    for (unsigned cycle=0;cycle<1000;++cycle) {
        const bool cinema = cycle%100 < 50;
        for (unsigned i=0;i<3;++i) { const double* p=s.in[i].data(); CHECK(s.fifo[i].push(&p,1024,1)); }
        const auto result = pollProgramSources(s,true,cinema);
        CHECK(result.musicAvailable == !cinema && result.auxAvailable == !cinema);
        CHECK(result.cinemaAvailable == cinema);
        if (cinema) CHECK(result.cinema.channel[0][0] == 3.0);
        else { CHECK(result.music.channel[0][0] == 1.0); CHECK(result.aux.channel[0][0] == 2.0); }
        for (auto& f : s.fifo) { CHECK(f.telemetry().fill_frames == 0); CHECK(f.telemetry().overflows == 0); }
    }
    std::puts("PASS 1000 cycles: every input drained, only selected program exposed, exact samples");
}
