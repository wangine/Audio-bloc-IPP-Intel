#include "../tests/TestSupport.h"
#include "../kernel/WaveRtTimeline.h"
#include <vector>
#include <iostream>
using namespace entropy::music;
void runTimelineContracts(){
 constexpr uint64_t hz=10000000;constexpr uint32_t packet=512,capacity=1024;
 for(uint32_t rate:{44100u,48000u,88200u,96000u,176400u,192000u})for(uint32_t bits:{16u,32u}){
  const auto fmt=format(rate,bits);
  std::vector<uint8_t> dma(capacity*fmt.blockAlign),pendingBytes(dma.size()),readyBytes(dma.size());
  std::vector<RingSlot> pendingSlots(8),readySlots(8);PcmRing pending,ready;WaveRtTimeline t;
  CHECK(pending.init(pendingBytes.data(),capacity,fmt.blockAlign,pendingSlots.data(),8));
  CHECK(ready.init(readyBytes.data(),capacity,fmt.blockAlign,readySlots.data(),8));
  CHECK(t.prepare(fmt,7,hz,dma.data(),capacity,2,pending,ready));
  auto fill=[&](uint32_t number){for(uint32_t b=0;b<packet*fmt.blockAlign;++b)
   dma[(number%2)*packet*fmt.blockAlign+b]=static_cast<uint8_t>((b*17+number*37)&255);};
  fill(0);CHECK(t.capturePacket(0,false,0,0)==CommitResult::Accepted);CHECK(t.start(0));
  CHECK(t.acquireClock(0,7,99,101)==FollowResult::Ok);
  uint64_t now=0,sequence=0;
  for(uint32_t number=0;number<128;++number){
   const uint64_t wanted=uint64_t(number+1)*packet;auto clock=t.clockSnapshot();
   uint64_t lo=now,hi=now+hz;
   while(hi-lo>1){const auto middle=lo+(hi-lo)/2;uint64_t position=0;CHECK(clock.position(middle,position));
    if(position>=wanted)hi=middle;else lo=middle;}
   now=hi;CHECK(t.advance(now,false));CHECK(t.frames()==wanted);CHECK(t.packetCount()==number+1);
   PacketView v{};CHECK(t.beginRead(v));CHECK(v.packet.firstSourceFrame==uint64_t(number)*packet);CHECK(v.packet.frames==packet);
   std::vector<uint8_t> actual(packet*fmt.blockAlign);std::memcpy(actual.data(),v.first,v.firstBytes);
   if(v.secondBytes)std::memcpy(actual.data()+v.firstBytes,v.second,v.secondBytes);
   for(uint32_t b=0;b<actual.size();++b)CHECK(actual[b]==static_cast<uint8_t>((b*17+number*37)&255));
   CHECK(t.finishRead(v.packet.sequence,true));
   if(number%8==0){CHECK(t.updateClock(now,7,99,101,++sequence,100000)==FollowResult::Ok);
    uint64_t after=0;CHECK(t.clockSnapshot().position(now,after)&&after==wanted);}
   if(number+1<128){fill(number+1);CHECK(t.capturePacket(number+1,false,0,now)==CommitResult::Accepted);}
  }
  CHECK(t.totals().lostFrames==0);CHECK(t.totals().publishedFrames==128*packet);
  CHECK(t.pause(now));CHECK(t.start(now+hz));
  CHECK(t.updateClock(now+hz,7,99,101,++sequence,0)==FollowResult::Stale);
  CHECK(t.requestStop(now+hz));
 }
 std::cout<<"Timeline contracts PASS: all 12 formats, exact PCM bytes, packet/frame counts and invalidation while retuning.\n";
}
