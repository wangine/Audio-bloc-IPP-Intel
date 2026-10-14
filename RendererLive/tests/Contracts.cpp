#include "RendererLive/AudioQueues.h"
#include "RendererLive/RateControl.h"
#include "RendererLive/WdlClockAsrc.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace entropy::live;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() try {
 AudioQueue q;require(q.prepare(2,8192),"queue prepare");
 std::array<double,1024> l{},r{};for(unsigned i=0;i<1024;++i){l[i]=double(i)/2048;r[i]=-l[i];}
 const double* in[]={l.data(),r.data()};std::vector<double> interleaved(2048);
 require(q.push(in,1024,100000,1),"publish first");require(!q.push(in,1024,102049,1),"reject gap");
 require(q.popInterleaved(interleaved.data(),1024),"read first");
 for(unsigned i=0;i<1024;++i){require(std::memcmp(&interleaved[2*i],&l[i],8)==0,"L exact");require(std::memcmp(&interleaved[2*i+1],&r[i],8)==0,"R exact");}
 for(unsigned b=1;b<33;++b){require(q.push(in,1024,100000+1024*b,1),"wrap publish");require(q.popInterleaved(interleaved.data(),1024),"wrap read");}
 require(q.published()==33792&&q.readable()==0,"queue counts");
 ReadyQueue ready;require(ready.prepare(2,32,1024,4),"ready prepare");
 auto* p=ready.beginWrite();require(p!=nullptr,"reserve packet");p->sourceBegin=0;p->sourceEnd=1024;p->frames=1024;p->bytes[17]=91;
 ready.commitWrite();const auto* got=ready.beginRead();require(got&&got->bytes[17]==91,"read packet");
 require(ready.beginRead()==got&&ready.size()==1,"failed submit retains packet");ready.commitRead();require(ready.size()==0,"successful submit retires packet");
 RateControl rate;rate.prepare(8192);double produced=8192,consumed=0;double drift=150e-6;double maxLevel=0;
 for(unsigned b=0;b<192000u*120u/1024u;++b){double ratio=rate.ratio();produced+=1024*(1+drift);consumed+=1024*ratio;const double level=produced-consumed;maxLevel=std::max(maxLevel,level);rate.observe(level,1024);}
 require(std::abs((rate.ratio()-1)*1e6-150)<100,"slow PI follows real drift");require(maxLevel<16000,"drift remains bounded");
 // Constant-ratio output must not depend on input packet boundaries.
 WdlClockAsrc a,b;require(a.prepare(2,1024)&&b.prepare(2,1024),"WDL prepare");
 AudioQueue qa,qb;require(qa.prepare(2,8192)&&qb.prepare(2,8192),"ASRC queues");
 std::array<double,4096> sa{},sb{};for(unsigned i=0;i<4096;++i){sa[i]=.1*std::sin(i*.1);sb[i]=-.2*std::cos(i*.05);}
 const double* all[]={sa.data(),sb.data()};require(qa.push(all,4096,0,1),"one input packet");
 for(unsigned i=0;i<4096;i+=256){const double* part[]={sa.data()+i,sb.data()+i};require(qb.push(part,256,i,1),"split input packets");}
 std::vector<double> oa(2048),ob(2048);
 for(unsigned k=0;k<3;++k){require(a.process(qa,oa.data(),1024,1.00015),"ASRC A");require(b.process(qb,ob.data(),1024,1.00015),"ASRC B");require(std::memcmp(oa.data(),ob.data(),oa.size()*8)==0,"packetization invariant");for(double v:oa)require(std::isfinite(v)&&std::abs(v)<.5,"finite bounded PCM");}
 std::cout<<"Live renderer contracts PASS: exact queue/order, commit-after-submit, bounded drift, WDL packetization. No hardware opened.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
