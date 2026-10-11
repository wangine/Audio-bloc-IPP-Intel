#include "TestSupport.h"
#include "../model/Clock.h"
#include "../model/PacketTimeline.h"
#include <vector>
#include <limits>
#include <intrin.h>
using namespace entropy::music;
void FormatRejectsUnsupported() {
    for(auto rate:{44100u,48000u,88200u,96000u,176400u,192000u}) for(auto bits:{16u,32u}) {
        auto f=format(rate,bits); CHECK(validateFormat(f)); uint32_t bytes=0;
        CHECK(payloadSize(f,MaxPacketFrames,bytes)); CHECK(bytes==MaxPacketFrames*f.blockAlign);
        CHECK(!payloadSize(f,0,bytes)); CHECK(!payloadSize(f,MaxPacketFrames+1,bytes));
    }
    for(int mutation=0;mutation<13;++mutation) {
        auto f=format(48000,32);
        switch(mutation) {case 0:f.subFormat[0]=3;break;case 1:f.validBits=32;break;case 2:f.containerBits=24;break;
        case 3:f.channelMask=0;break;case 4:f.channels=8;break;case 5:f.sampleRate=22050;break;
        case 6:f.averageBytesPerSecond++;break;case 7:f.blockAlign=UINT32_MAX;break;case 8:f.structSize--;break;
        case 9:f.reserved[1]=1;break;case 10:f.representation=2;break;case 11:f.formatTag=3;break;case 12:f.subFormat[15]^=1;break;}
        CHECK(!validateFormat(f));
    }
}
void ClockFractionalRatesAndPause() {
    for(auto rate:{44100u,48000u,88200u,96000u,176400u,192000u}) {
        Clock c; CHECK(c.reset(rate,10000003,1)); CHECK(c.start(99)); uint64_t n=0;
        for(uint64_t t=0;t<=10000;++t) {CHECK(c.position(99+t*1000,n));CHECK(n==((t*1000)*rate)/10000003);}
        CHECK(c.position(10000102,n));CHECK(n==rate); CHECK(c.pause(10000102));
        CHECK(c.position(40000000,n));CHECK(n==rate);CHECK(c.start(40000000));
        CHECK(c.position(50000003,n));CHECK(n==2*rate);
    }
    Clock c;uint64_t n=777;CHECK(!c.reset(44100,0,1));CHECK(!c.reset(0,10,1));
    CHECK(c.reset(44100,1,1));CHECK(c.start(0));CHECK(!c.position(UINT64_MAX,n));CHECK(n==777);
    CHECK(c.reset(44100,UINT64_MAX,1));CHECK(c.start(0));CHECK(c.position(UINT64_MAX,n));CHECK(n==44100);
    CHECK(multiplyDivide(UINT64_MAX-1,192000,UINT64_MAX,n));CHECK(n==191999);
    // Repeated tiny pause/resume operations must preserve fractional QPC time.
    CHECK(c.reset(44100,10000003,1));uint64_t tick=100;
    for(unsigned i=0;i<10000;++i){CHECK(c.start(tick));CHECK(c.pause(tick+1000));tick+=2000;}
    CHECK(c.position(tick,n));CHECK(n==(10000000ull*44100)/10000003);
}
struct Fixture {
    std::vector<uint8_t> pm,rm;
    std::vector<RingSlot> ps,rs;
    PcmRing pending,ready;PacketTimeline timeline;
    Fixture(uint32_t capacity=8,uint32_t descriptors=4):pm(capacity*4),rm(capacity*4),ps(descriptors),rs(descriptors) {
        CHECK(pending.init(pm.data(),capacity,4,ps.data(),descriptors));
        CHECK(ready.init(rm.data(),capacity,4,rs.data(),descriptors));
        CHECK(timeline.prepare(format(),1,10000000,pending,ready));
    }
};
PacketDescriptor desc(uint64_t seq,uint64_t first,uint32_t frames,uint64_t gen=1){return {gen,seq,first,100,10000000,0,frames,RawIdentity,0,0};}
const uint8_t pattern[32]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};
void PrefillSurvivesRun() {
    Fixture f;PacketView view{};CHECK(f.timeline.commit(desc(1,0,2),pattern,8)==CommitResult::Accepted);
    CHECK(!f.ready.peek(view));CHECK(f.timeline.advance(1));CHECK(!f.ready.peek(view));
    CHECK(f.timeline.advance(2));CHECK(f.ready.peek(view));CHECK(view.packet.firstSourceFrame==0);
    uint8_t out[8]{};uint32_t copied=0;CHECK(f.ready.copyFront(out,8,copied));CHECK(std::memcmp(out,pattern,8)==0);
    CHECK(f.timeline.commit(desc(1,0,2),pattern,8)==CommitResult::Duplicate);
    CHECK(f.timeline.advance(2));CHECK(f.ready.count()==1);CHECK(!f.ready.consume(2));CHECK(f.ready.consume(1));
    CHECK(f.timeline.advance(2));CHECK(!f.ready.peek(view));
}
void RingWrapAndLateReader() {
    Fixture f(4,3);uint8_t out[16]{};uint32_t copied=0;PacketView v{};
    CHECK(f.timeline.commit(desc(1,0,3),pattern,12)==CommitResult::Accepted);CHECK(f.timeline.advance(3));CHECK(f.ready.consume(1));
    CHECK(f.timeline.commit(desc(2,3,2),pattern+12,8)==CommitResult::Accepted);CHECK(f.timeline.advance(5));
    CHECK(f.ready.peek(v));CHECK(v.packet.firstSourceFrame==3);CHECK(v.secondBytes==4);
    CHECK(f.ready.copyFront(out,sizeof(out),copied));CHECK(std::memcmp(out,pattern+12,8)==0);
    CHECK(f.timeline.commit(desc(3,5,3),pattern,12)==CommitResult::Accepted);CHECK(f.timeline.advance(8));
    CHECK(f.timeline.totals().lostFrames==3);CHECK(f.ready.peek(v));CHECK(v.packet.sequence==2);
    CHECK(f.ready.consume(2));CHECK(f.timeline.commit(desc(4,8,1),pattern+24,4)==CommitResult::Accepted);CHECK(f.timeline.advance(9));
    CHECK(f.ready.peek(v));CHECK(v.packet.firstSourceFrame==8 && v.packet.lostFrames==3 && (v.packet.flags&Discontinuity));
    CHECK(f.ready.copyFront(out,sizeof(out),copied));CHECK(std::memcmp(out,pattern+24,4)==0);
    f.timeline.stop();CHECK(f.timeline.terminatedTotals().lostFrames==3);CHECK(f.timeline.lifetimeTotals().committedFrames==9);
    CHECK(f.timeline.prepare(format(),2,10000000,f.pending,f.ready));CHECK(f.timeline.lifetimeTotals().lostFrames==3);
    PcmRing bad;CHECK(!bad.init(out,RingFrames+1,4,f.rs.data(),3));CHECK(!bad.init(out,4,4,f.rs.data(),RingDescriptors+1));
}
void DescriptorAndPendingOverrun() {
    Fixture f(8,1);
    CHECK(f.timeline.commit(desc(1,0,2),pattern,8)==CommitResult::Accepted);
    CHECK(f.timeline.commit(desc(2,2,2),pattern+8,8)==CommitResult::Overrun);
    CHECK(f.timeline.totals().lostFrames==2);CHECK(f.timeline.advance(4));CHECK(f.ready.consume(1));
    CHECK(f.timeline.commit(desc(3,4,1),pattern,4)==CommitResult::Accepted);CHECK(f.timeline.advance(5));
    PacketView v{};CHECK(f.ready.peek(v));CHECK(v.packet.lostFrames==2 && v.packet.firstSourceFrame==4);
}
void StopRestartAndPartialEos() {
    Fixture f; auto tail=desc(1,0,1);tail.flags|=EndOfStream;
    CHECK(f.timeline.commit(tail,pattern,4)==CommitResult::Accepted);CHECK(f.timeline.eos());
    CHECK(f.timeline.advance(1));PacketView v{};CHECK(f.ready.peek(v));CHECK(v.packet.frames==1);CHECK(f.ready.consume(1));
    f.timeline.stop();CHECK(f.timeline.commit(desc(2,1,1),pattern,4)==CommitResult::Stopped);
    CHECK(f.timeline.prepare(format(),2,10000000,f.pending,f.ready));
    CHECK(f.timeline.commit(desc(1,0,1),pattern,4)==CommitResult::Invalid);
    auto zero=desc(1,0,0,2);zero.flags|=EndOfStream;
    CHECK(f.timeline.commit(zero,nullptr,0)==CommitResult::Accepted);CHECK(f.timeline.eos());CHECK(!f.ready.peek(v));
    PacketNumber pn;uint64_t n=0;CHECK(pn.extend(0xfffffffe,n));CHECK(n==0xfffffffeull);
    CHECK(pn.extend(0xffffffff,n));CHECK(pn.extend(0,n));CHECK(n==0x100000000ull);
    CHECK(!pn.extend(0,n));CHECK(!pn.extend(0xffffffff,n));
}
void RejectInvalidAndLateWithoutInventedData() {
    Fixture f;CHECK(f.timeline.advance(3));CHECK(f.timeline.commit(desc(1,0,2),pattern,8)==CommitResult::Late);
    CHECK(f.timeline.totals().lostFrames==2);CHECK(f.timeline.commit(desc(2,2,1),pattern,3)==CommitResult::Invalid);
    CHECK(f.timeline.totals().committedFrames==2);CHECK(f.timeline.commit(desc(2,2,1),pattern,4)==CommitResult::Late);
    CHECK(f.timeline.commit(desc(3,3,1),pattern,4)==CommitResult::Accepted);CHECK(f.timeline.advance(4));
    PacketView v{};CHECK(f.ready.peek(v));CHECK(v.packet.firstSourceFrame==3 && v.packet.lostFrames==3);
    CHECK(!f.timeline.advance(2));
}
void ClockComparedToWideIntegerOracle() {
    uint64_t seed=0x14e83638dc5e73a1;
    auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
    for(unsigned i=0;i<100000;++i) {
        const uint64_t ticks=random(),frequency=random()|1;
        const uint32_t rate=static_cast<uint32_t>(random()%192000)+1;
        uint64_t high=0,remainder=0,actual=0;
        const auto low=_umul128(ticks,rate,&high);
        const bool fits=high<frequency;
        CHECK(multiplyDivide(ticks,rate,frequency,actual)==fits);
        if(fits)CHECK(actual==_udiv128(high,low,frequency,&remainder));
    }
}
void ImmutableBytesAndAllFormatsAcrossWrap() {
    for(auto rate:{44100u,48000u,88200u,96000u,176400u,192000u})for(auto bits:{16u,32u}) {
        const auto fmt=format(rate,bits);constexpr uint32_t capacity=127;
        std::vector<uint8_t> pm(capacity*fmt.blockAlign),rm(pm.size());std::vector<RingSlot> ps(7),rs(7);
        PcmRing pending,ready;PacketTimeline t;
        CHECK(pending.init(pm.data(),capacity,fmt.blockAlign,ps.data(),7));CHECK(ready.init(rm.data(),capacity,fmt.blockAlign,rs.data(),7));
        CHECK(t.prepare(fmt,1,10000000,pending,ready));uint64_t first=0;
        for(uint64_t seq=1;seq<=1000;++seq) {
            const uint32_t frames=static_cast<uint32_t>(seq%31)+1,bytes=frames*fmt.blockAlign;
            std::vector<uint8_t> input(bytes),expected(bytes),output(bytes);
            for(uint32_t byte=0;byte<bytes;++byte)input[byte]=static_cast<uint8_t>((seq*17+byte*53)&255);
            expected=input;CHECK(t.commit(desc(seq,first,frames),input.data(),bytes)==CommitResult::Accepted);
            std::memset(input.data(),0xba,bytes);CHECK(t.advance(first+frames));
            uint32_t copied=0;CHECK(ready.copyFront(output.data(),bytes,copied));CHECK(output==expected);
            PacketView v{};CHECK(ready.peek(v));CHECK(v.packet.firstSourceFrame==first && v.packet.lostFrames==0);
            CHECK(ready.consume(seq));first+=frames;
        }
        CHECK(t.totals().committedFrames==first && t.totals().publishedFrames==first && t.totals().lostFrames==0);
    }
}
int main(){const Case cases[]={{"FormatRejectsUnsupported",FormatRejectsUnsupported},{"ClockFractionalRatesAndPause",ClockFractionalRatesAndPause},
{"PrefillSurvivesRun",PrefillSurvivesRun},{"RingWrapAndLateReader",RingWrapAndLateReader},{"DescriptorAndPendingOverrun",DescriptorAndPendingOverrun},
{"StopRestartAndPartialEos",StopRestartAndPartialEos},{"RejectInvalidAndLateWithoutInventedData",RejectInvalidAndLateWithoutInventedData},
{"ClockComparedToWideIntegerOracle",ClockComparedToWideIntegerOracle},{"ImmutableBytesAndAllFormatsAcrossWrap",ImmutableBytesAndAllFormatsAcrossWrap}};return runTests(cases,sizeof(cases)/sizeof(cases[0]));}
