#include "RendererLive/DomainCore.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace entropy::live;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main() try {
 DomainCore d;check(d.prepare(8,1,12345,true),"original PCM24 configuration");check(d.setQuantum(1024),"quantum");
 std::array<std::array<double,1024>,8> input{};std::array<const double*,8> p{};
 for(unsigned c=0;c<8;++c){p[c]=input[c].data();for(unsigned f=0;f<1024;++f)input[c][f]=.01*(c+1)*std::sin(f*.03);}
 for(unsigned b=0;b<12;++b)check(d.input.push(p.data(),1024,uint64_t(b)*1024,1),"input publish");
 const auto before=d.pendingSourceFrames();check(before==12288,"initial total backlog");
 for(unsigned b=0;b<4;++b)check(d.produce()==DomainCore::Production::Made,"prepare packet");
 check(d.pendingSourceFrames()==before,"internal transfer cannot hide backlog");
 const auto* packet=d.ready.beginRead();check(packet&&packet->frames==1024,"prepared block");
 for(unsigned n=0;n<d.bytesPerPacket();n+=4)check(packet->bytes[n]==0,"PCM24 left alignment");
 check(d.submitted(),"commit after accept");check(d.pendingSourceFrames()==before-1024,"only accepted output reduces backlog");
 check(d.ready.size()==3,"queue retirement");check(d.pcmTelemetry().nonfinite_sample_count==0,"finite output");
 std::cout<<"Domain contracts PASS: original PCM24, all-queue accounting, no early consumption.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
