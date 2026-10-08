#include "Engine/Egress/SpscAudioFifo.h"
#include <array>
#include <bit>
#include <cstdio>
#include <thread>
using entropy::egress_host::SpscAudioFifo;
int main() {
    SpscAudioFifo q;
    if(q.prepare(2,7) || !q.prepare(2,8)) return 1;
    std::array<double,8> a{0.0,-0.0,1,2,3,4,5,6}, b=a, x{},y{};
    const double* in[]={a.data(),b.data()}; double* out[]={x.data(),y.data()};
    if(!q.push(in,8) || q.push(in,1) || q.overflows()!=1) return 2;
    if(!q.pop(out,7) || !q.push(in,7) || !q.pop(out,8) || x[0]!=6 || !std::signbit(x[2])) return 3;
    if(q.pop(out,1) || q.underflows()!=1) return 4;
    q.resetQuiescent();
    constexpr unsigned count=200000;
    std::thread producer([&]{for(unsigned i=0;i<count;){double v=i;const double* p[]={&v,&v};if(q.push(p,1))++i;}});
    for(unsigned i=0;i<count;){double u=0,v=0;double* p[]={&u,&v};if(q.pop(p,1)){if(u!=i||v!=i) std::abort();++i;}}
    producer.join();
    std::puts("PASS FIFO wrap/signed-zero/overflow/underflow/concurrent 200000 frames");
}
