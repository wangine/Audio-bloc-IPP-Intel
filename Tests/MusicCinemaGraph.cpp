#include "MusicCinemaFixture.h"
#include "Engine/AudioGraph.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
int main() try {
 namespace audit=host::allocation_audit;
 check(audit::installHooks(),"RT audit hooks installed");
 for(unsigned layout:{4u,5u,6u}){
  Source music(4,71,0,.000001),cinema(layout==4?5:layout,72,0,.000001);input::ProgramIngress in;check(in.start(music.endpoint,cinema.endpoint,false),"differential input starts");
  engine::AudioGraph candidate,reference;check(candidate.prepare(1024)&&reference.prepare(1024),"0.1.8 Graph prepared twice");
  for(auto* graph:{&candidate,&reference}){check(graph->setOfflineParameter("cinemaSource",layout==4?0:1),"baseline program");check(graph->applyOfflineParameters(true),"baseline parameters applied");}
  engine::PlanarBuffer mapped,legacy,a,b;mapped.prepare(13,1024);legacy.prepare(13,1024);a.prepare(16,1024);b.prepare(16,1024);legacy.clear(1024);
  for(unsigned c=0;c<3;++c)memcpy(legacy.channel(c),music.audio[c].data(),8192);for(unsigned c=0;c<cinema.session.channels;++c)memcpy(legacy.channel(c+3),cinema.audio[c].data(),8192);
  if(layout==4)for(unsigned c=3;c<13;++c)std::fill_n(legacy.channel(c),1024,0.0);else for(unsigned c=0;c<3;++c)std::fill_n(legacy.channel(c),1024,0.0);
  uint64_t compared=0,nonzero=0;audit::reset();
  while(compared<32){input::GraphInputContext context;bool received=false;{audit::CycleScope scope;received=in.tryRead(mapped.writePointers(),layout==4?1:2,context);}if(!received){Sleep(1);continue;}
   for(unsigned c=0;c<13;++c)check(!memcmp(mapped.channel(c),legacy.channel(c),8192),"new mapping equals independent legacy reference");
   engine::GraphControlAccess::crossover(candidate)->setProgramInputEpochs(context.music.token,context.music.token,context.cinema.token,0xffff);
   engine::GraphControlAccess::crossover(reference)->setProgramInputEpochs(layout==4?91:0,layout==4?23:0,layout==4?0:23,0xffff);
   EntropyResult r;{audit::CycleScope scope;r=candidate.process(mapped.readPointers(),a.writePointers(),1024);}check(r==ENTROPY_OK,"candidate Graph processing");check(reference.process(legacy.readPointers(),b.writePointers(),1024)==ENTROPY_OK,"reference Graph processing");
   for(unsigned c=0;c<16;++c)for(unsigned f=0;f<1024;++f){check(!memcmp(&a.channel(c)[f],&b.channel(c)[f],8),"0.1.8 Graph output bit exact");nonzero+=a.channel(c)[f]!=0;}
   ++compared;
  }
  auto rt=audit::snapshot();check(!rt.cppNew&&!rt.cppDelete&&!rt.heapAlloc&&!rt.heapFree&&!rt.wait&&!rt.lock&&!rt.io,"production ingress and Graph RT phase without allocation, blocking wait/lock or IO");check(nonzero>0,"differential Graph comparison includes nonzero output");in.stop();
 }
 std::cout<<"PASS nonzero Graph differential Music3/Cinema6/8, bit-exact output, RT allocation/wait/lock/IO audit\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
