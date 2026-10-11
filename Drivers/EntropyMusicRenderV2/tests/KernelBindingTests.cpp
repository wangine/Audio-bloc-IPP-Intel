#include "TestSupport.h"
#include "../kernel/WaveRtTimeline.h"
#include "../kernel/OperationGate.h"
#include <thread>
using namespace entropy::music;
struct Binding {
    uint8_t dma[32]{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},pbuf[32]{},rbuf[64]{};
    RingSlot ps[4]{},rs[8]{};PcmRing pending,ready;WaveRtTimeline t;
    explicit Binding(uint32_t notifications=2) {
        CHECK(pending.init(pbuf,8,4,ps,4));CHECK(ready.init(rbuf,16,4,rs,8));
        CHECK(t.prepare(format(48000),1,48000,dma,8,notifications,pending,ready));
    }
};
void KernelBindingsKeepPrefill(){
    Binding b;CHECK(b.t.capturePacket(0,false,0,100)==CommitResult::Accepted);
    std::memset(b.dma,0xba,16);CHECK(b.t.start(100));CHECK(b.t.advance(104,false));
    PacketView v{};CHECK(b.t.beginRead(v));CHECK(v.packet.firstSourceFrame==0 && v.first[0]==1 && v.first[15]==16);
    CHECK(b.t.finishRead(1,true));CHECK(b.t.packetCount()==1);
}
void NoDualProducer(){
    Binding b;CHECK(b.t.capturePacket(0,false,0,100)==CommitResult::Accepted);CHECK(b.t.start(100));
    for(unsigned i=0;i<10;++i)CHECK(b.t.advance(104,i%2!=0));
    CHECK(b.t.totals().publishedPackets==1 && b.t.packetCount()==1);
    CHECK(b.t.capturePacket(0,false,0,104)==CommitResult::Duplicate);
    CHECK(b.t.totals().committedPackets==1);
}
void AllocationModeIsNotWasapiFlag(){
    Binding packet;Binding position(0);CHECK(packet.t.owner()==BufferOwner::Packet && position.t.owner()==BufferOwner::Position);
    CHECK(position.t.capturePacket(0,false,0,100)==CommitResult::Invalid);
    CHECK(packet.t.start(100));CHECK(packet.t.pause(102));CHECK(packet.t.advance(120,false));CHECK(packet.t.frames()==2);
    CHECK(packet.t.start(200));CHECK(packet.t.advance(202,false));CHECK(packet.t.frames()==4);
}
void PositionDrivenReadOwnership(){
    Binding b(0);CHECK(b.t.start(100));CHECK(b.t.advance(102,false));CHECK(b.ready.count()==0);
    CHECK(b.t.advance(104,true));CHECK(b.ready.count()==1);
    std::memset(b.dma,0,16);PacketView v{};CHECK(b.t.beginRead(v));CHECK(v.first[0]==1 && v.packet.frames==4);
    CHECK(b.t.finishRead(1,true));CHECK(b.t.advance(104,true));CHECK(b.ready.count()==0);
    // A full unseen lap is ambiguous: reject, never certify overwritten bytes.
    CHECK(!b.t.advance(120,true));CHECK(b.ready.count()==0);
}
void StopDuringPendingCommit(){
    Binding b;CHECK(b.t.capturePacket(0,false,0,100)==CommitResult::Accepted);CHECK(b.t.start(100));CHECK(b.t.advance(104,false));
    PacketView v{};CHECK(b.t.beginRead(v));CHECK(!b.t.requestStop(104));CHECK(b.t.readActive() && b.ready.count()==1);
    CHECK(b.t.capturePacket(1,false,0,104)==CommitResult::Stopped);CHECK(!b.t.start(104));
    CHECK(v.first[0]==1);CHECK(b.t.finishRead(1,false));CHECK(!b.t.prepared() && b.ready.count()==0);
    CHECK(b.t.prepare(format(48000),2,48000,b.dma,8,2,b.pending,b.ready));CHECK(b.t.packetCount()==0);
}
void ExactEosAndInvalidBufferBounds(){
    Binding b;CHECK(b.t.capturePacket(0,true,12,100)==CommitResult::Accepted);CHECK(b.t.eos());
    CHECK(b.t.start(100));CHECK(b.t.advance(103,false));PacketView v{};CHECK(b.t.beginRead(v));CHECK(v.packet.frames==3);CHECK(b.t.packetCount()==1);
    CHECK(b.t.finishRead(1,true));CHECK(b.t.advance(104,false));CHECK(b.ready.count()==0);
    CHECK(!b.t.prepare(format(48000),2,48000,b.dma,7,2,b.pending,b.ready));
}
void ProducerNeverWaitsForAnotherOwner(){
    OperationGate gate;CHECK(gate.tryEnter());bool busyRejected=false;
    std::thread competing([&](){busyRejected=!gate.tryEnter();});competing.join();CHECK(busyRejected);
    gate.leave();CHECK(gate.tryEnter());gate.leave();
    uint64_t counter=0;
    auto contender=[&](){for(unsigned i=0;i<10000;++i){while(!gate.tryEnter())std::this_thread::yield();++counter;gate.leave();}};
    std::thread first(contender),second(contender);first.join();second.join();CHECK(counter==20000);
}
int main(){const Case cases[]={{"KernelBindingsKeepPrefill",KernelBindingsKeepPrefill},{"NoDualProducer",NoDualProducer},
{"AllocationModeIsNotWasapiFlag",AllocationModeIsNotWasapiFlag},{"PositionDrivenReadOwnership",PositionDrivenReadOwnership},
{"StopDuringPendingCommit",StopDuringPendingCommit},{"ExactEosAndInvalidBufferBounds",ExactEosAndInvalidBufferBounds},
{"ProducerNeverWaitsForAnotherOwner",ProducerNeverWaitsForAnotherOwner}};return runTests(cases,7);}
