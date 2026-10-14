#include "Engine/Input/IngressConcurrency.h"
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
using namespace entropy::input;
struct Payload {uint64_t id=0,session=0;std::array<double,1024> pcm{};};
using Queue=IngressSpscQueue<Payload,16>;
void check(bool ok,const char* what){if(!ok){std::cerr<<"FAIL "<<what<<'\n';std::abort();}}
void push(Queue& q,uint64_t id){auto* p=q.producerSlot();check(p,"slot available");p->id=id;p->session=id/16;p->pcm.fill(double(id));p->pcm[0]=-0.0;p->pcm[1]=std::bit_cast<double>(uint64_t{1});q.commitProducer();}
void queueCases(){auto owner=std::make_unique<Queue>();auto& q=*owner;
 check(q.acquireConsumer(),"initial admission");check(!q.front(),"empty");q.releaseConsumer();
 for(uint64_t i=0;i<16;++i){push(q,i);}check(!q.producerSlot(),"full never overwrite");
 for(uint64_t i=0;i<16;++i){check(q.acquireConsumer(),"full admission");auto* p=q.front();check(p&&p->id==i,"FIFO order");check(std::bit_cast<uint64_t>(p->pcm[0])==0x8000000000000000ULL,"signed zero");check(std::bit_cast<uint64_t>(p->pcm[1])==1,"subnormal bits");q.popConsumer();q.releaseConsumer();}
 for(uint64_t i=0;i<1031;++i){push(q,i);check(q.acquireConsumer(),"wrap admission");check(q.front()->id==i,"wrap");q.popConsumer();q.releaseConsumer();}
 push(q,2000);check(q.acquireConsumer(),"pin");auto* pinned=q.front();
 check(!q.beginMaintenance(),"maintenance must not own pinned PCM");
 check(pinned->id==2000,"pinned data intact");q.releaseConsumer();
 check(!q.acquireConsumer(),"new reader excluded by maintenance request");
 check(q.beginMaintenance(),"reader acknowledgement");check(q.discardExclusive()==1,"explicit retirement");q.endMaintenance();
 check(q.acquireConsumer(),"reader admitted after retirement");check(!q.front(),"retired data not replayed");q.releaseConsumer();push(q,3000);
 check(q.acquireConsumer(),"new stream admission");check(q.front()->id==3000,"new stream data");q.popConsumer();q.releaseConsumer();
 std::cout<<"PASS FIFO/capacity/wrap/bit-preservation/lifecycle\n";
}
struct Observation {uint64_t version=0;std::array<uint64_t,64> words{};};
void snapshotCases(){LatestIngressTelemetry<Observation> t;std::atomic<bool> done{false};std::atomic<uint64_t> copies{0};
 std::array<std::thread,3> readers;for(auto& r:readers)r=std::thread([&]{do{auto v=t.read();for(auto x:v.words)check(x==v.version,"coherent telemetry");++copies;}while(!done.load());});
 for(uint64_t n=1;n<=50000;++n){Observation v;v.version=n;v.words.fill(n);t.publish(v);}
 done=true;for(auto& r:readers)r.join();auto v=t.read();check(v.version==50000,"latest telemetry");std::cout<<"PASS telemetry coherent multi-reader copies="<<copies<<'\n';
}
struct BlockingCopy {
 uint64_t value=0;static inline std::atomic<bool> hold{false},entered{false};
 BlockingCopy()=default;BlockingCopy(const BlockingCopy& other):value(other.value){entered=true;while(hold.load())std::this_thread::yield();}
 BlockingCopy& operator=(const BlockingCopy&)=default;
};
void pausedObserver(){LatestIngressTelemetry<BlockingCopy> t;BlockingCopy v;v.value=1;t.publish(v);BlockingCopy::hold=true;
 std::thread reader([&]{auto copy=t.read();check(copy.value==1,"held snapshot immutable");});while(!BlockingCopy::entered.load())std::this_thread::yield();
 auto owner=std::make_unique<Queue>();auto& q=*owner;for(uint64_t n=0;n<10000;++n){v.value=n+2;t.publish(v);push(q,n);check(q.acquireConsumer(),"stalled telemetry must not reject audio");check(q.front()->id==n,"audio during stalled telemetry");q.popConsumer();q.releaseConsumer();}
 BlockingCopy::hold=false;reader.join();check(t.read().value==10001,"latest after stalled observer");std::cout<<"PASS stalled telemetry: 10000 audio blocks unaffected\n";
}
void concurrentQueue(){auto owner=std::make_unique<Queue>();auto& q=*owner;constexpr uint64_t total=100000;std::atomic<bool> go{false};
 std::thread producer([&]{while(!go.load()){}for(uint64_t i=0;i<total;){if(auto* p=q.producerSlot()){p->id=i;p->session=~i;p->pcm.fill(double(i));q.commitProducer();++i;}else std::this_thread::yield();}});
 go=true;for(uint64_t i=0;i<total;){check(q.acquireConsumer(),"no normal producer admission lock");if(auto* p=q.front()){check(p->id==i&&p->session==~i,"concurrent order/header");for(double x:p->pcm)check(x==double(i),"no torn payload");q.popConsumer();++i;}q.releaseConsumer();}
 producer.join();std::cout<<"PASS concurrent 100000-block bit continuity\n";
}
int main(){queueCases();snapshotCases();pausedObserver();concurrentQueue();}
