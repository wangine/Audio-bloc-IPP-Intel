#include "Core/SourceFifo192.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>

namespace {
void require(bool v, const char* m) { if (!v) { std::cerr << "FAIL: " << m << '\n'; std::exit(1); } }
}

int main()
{
    entropy::host::SourceFifo192 fifo;
    require(fifo.prepare(2, 4096), "prepare stereo FIFO");

    std::array<std::vector<double>,2> in{std::vector<double>(3000),std::vector<double>(3000)};
    for (std::size_t n=0;n<3000;++n) { in[0][n]=double(n); in[1][n]=-double(n); }
    std::array<const double*,2> ip{in[0].data(),in[1].data()};
    require(fifo.reset(7), "reset generation 7");
    require(fifo.push(ip.data(), 3000, 7), "push generation 7");

    std::array<std::vector<double>,2> out{std::vector<double>(1024),std::vector<double>(1024)};
    std::array<double*,2> op{out[0].data(),out[1].data()};
    std::uint64_t generation=0;
    require(fifo.pop(op.data(),1024,generation) && generation==7, "pop exact block");
    for(std::size_t n=0;n<1024;++n)
        require(out[0][n]==double(n) && out[1][n]==-double(n), "channel/order integrity");

    auto before=fifo.telemetry();
    require(!fifo.pop(op.data(),2048,generation), "partial underrun must reject whole request");
    auto after=fifo.telemetry();
    require(after.consumed_frames==before.consumed_frames && after.underruns==before.underruns+1,
            "underrun consumed data");

    require(fifo.reset(8), "reset generation 8");
    require(fifo.telemetry().fill_frames==0, "reset did not clear fill");
    require(fifo.push(ip.data(),1024,8), "push generation 8");
    require(fifo.pop(op.data(),1024,generation) && generation==8, "new generation not preserved");

    require(fifo.reset(9), "reset generation 9");
    require(fifo.push(ip.data(),3000,9), "fill before overflow");
    require(!fifo.push(ip.data(),2000,9), "overflow must reject whole push");
    auto t=fifo.telemetry();
    require(t.fill_frames==3000 && t.overflows==1, "overflow altered unread data");
    require(t.high_water_frames>=3000 && t.low_water_frames<=t.fill_frames, "watermarks invalid");

    // A stream generation reset must not race a consumer's sample copy or
    // publication of its old read cursor. Removing reset exclusion breaks this.
    require(fifo.reset(10), "reset generation 10");
    std::atomic<bool> done{false}, corrupt{false};
    std::atomic<unsigned> reads{0};
    std::thread consumer([&] {
        while (!done.load()) {
            std::uint64_t g=0;
            if (!fifo.pop(op.data(),1024,g)) { std::this_thread::yield(); continue; }
            ++reads;
            for (unsigned n=0;n<1024;++n)
                if (out[0][n]!=double(g) || out[1][n]!=-double(g)) corrupt=true;
        }
    });
    for (std::uint64_t g=11;g<20011;++g) {
        while (!fifo.reset(g)) std::this_thread::yield();
        std::fill(in[0].begin(),in[0].end(),double(g));
        std::fill(in[1].begin(),in[1].end(),-double(g));
        require(fifo.push(ip.data(),3000,g), "fresh generation must accept samples");
        std::this_thread::yield();
    }
    done=true;
    consumer.join();
    require(reads>0, "concurrent consumer must exercise sample copies");
    require(!corrupt, "reset mixed samples/generation or republished an old cursor");
    require(fifo.telemetry().fill_frames<=4096, "reset corrupted FIFO occupancy");
    std::cout << "PASS SourceFifo192 including concurrent generation reset\n";
}
