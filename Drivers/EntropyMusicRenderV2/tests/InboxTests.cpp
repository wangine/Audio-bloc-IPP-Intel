#include "TestSupport.h"
#include "../model/CommitInbox.h"
#include "../kernel/AllocationPolicy.h"
#include "../kernel/OperationGate.h"
#include "../kernel/WaveRtTimeline.h"
#include "../kernel/PacketAdmission.h"
#include "../model/DriverLedger.h"
#include <thread>
#include <vector>
using namespace entropy::music;
void PublicationContentionPreservesCommit(){uint8_t bytes[32]{};InboxSlot slots[8]{};CommitInbox inbox;CHECK(inbox.init(bytes,8,4,slots,8));OperationGate publication;CHECK(publication.tryEnter());uint8_t input[16]{1,2,3,4};for(auto flags:{RawIdentity,RawIdentity|EndOfStream}){PacketDescriptor d{1,1,0,100,48000,0,4,flags,0,0};CHECK(inbox.push(d,input)==InboxResult::Stored);input[0]=99;InboxView v{};CHECK(inbox.peek(v));CHECK(v.pcm.first[0]==1 && !v.dropped);CHECK(inbox.consume(d.sequence));input[0]=1;}publication.leave();}
void FullPayloadRecordsKnownLoss(){uint8_t bytes[16]{};InboxSlot slots[4]{};CommitInbox inbox;CHECK(inbox.init(bytes,4,4,slots,4));uint8_t input[16]{};PacketDescriptor d{1,1,0,100,48000,0,4,RawIdentity,0,0};CHECK(inbox.push(d,input)==InboxResult::Stored);d.sequence=2;d.firstSourceFrame=4;CHECK(inbox.push(d,input)==InboxResult::RecordedLoss);InboxView v{};CHECK(inbox.peek(v) && !v.dropped);CHECK(inbox.consume(1));CHECK(inbox.peek(v) && v.dropped && v.pcm.packet.frames==4);CHECK(inbox.consume(2));CHECK(inbox.queuedFrames()==0);}
void ConcurrentSnapshotsAreImmutable(){std::vector<uint8_t> data(1024*4);std::vector<InboxSlot> slots(1024);CommitInbox inbox;CHECK(inbox.init(data.data(),1024,4,slots.data(),1024));bool exact=true;std::thread reader([&](){for(uint64_t i=1;i<=10000;++i){InboxView v{};while(!inbox.peek(v))std::this_thread::yield();uint32_t value=0;if(v.dropped || v.pcm.firstBytes!=4)exact=false;else std::memcpy(&value,v.pcm.first,4);if(value!=i)exact=false;if(!inbox.consume(i))exact=false;}});for(uint32_t i=1;i<=10000;++i){while(inbox.queuedRecords()>=512)std::this_thread::yield();PacketDescriptor d{1,i,i-1,100,48000,0,1,RawIdentity,0,0};CHECK(inbox.push(d,reinterpret_cast<uint8_t*>(&i))==InboxResult::Stored);}reader.join();CHECK(exact);}
void UnprovenAllocationIsRefused(){CHECK(!rawAllocationSupported(0));CHECK(rawAllocationSupported(1));CHECK(rawAllocationSupported(2));CHECK(!rawAllocationSupported(3));}
void ValidSnapshotCanBePublishedAfterClockElapsed(){uint8_t dma[32]{},pendingBytes[32]{},readyBytes[64]{};RingSlot ps[4]{},rs[8]{};PcmRing p,r;CHECK(p.init(pendingBytes,8,4,ps,4));CHECK(r.init(readyBytes,16,4,rs,8));WaveRtTimeline t;CHECK(t.prepare(format(48000),1,48000,dma,8,2,p,r));CHECK(t.start(100));CHECK(t.advance(110,false));uint8_t valid[16]{1,2,3};PacketDescriptor d{1,1,0,100,48000,0,4,RawIdentity,0,0};PacketView v{d,valid,nullptr,16,0};CHECK(t.captureSnapshot(v,false)==CommitResult::Accepted);CHECK(t.advance(110,false));PacketView out{};CHECK(t.beginRead(out));CHECK(out.first[0]==1 && !out.packet.lostFrames);CHECK(t.finishRead(1,true));}
void IndependentAdmission(){PacketAdmission p;p.reset(3,48000,4,1,4);PacketDescriptor d{};CHECK(p.accept(0,false,0,0,100,d)==CommitResult::Accepted);CHECK(d.firstSourceFrame==0);CHECK(p.accept(1,false,0,5,105,d)==CommitResult::Accepted);CHECK(p.accept(3,false,0,12,112,d)==CommitResult::Accepted);CHECK(d.firstSourceFrame==12);CHECK(p.accept(3,false,0,12,112,d)==CommitResult::Duplicate);CHECK(p.accept(4,true,8,16,116,d)==CommitResult::Accepted);CHECK(d.frames==2 && d.flags&EndOfStream);CHECK(p.accept(5,false,0,20,120,d)==CommitResult::Invalid);}
void LifetimeSurvivesAllocation(){DriverLedger ledger;Totals a{8,4,2,4,2,1};CHECK(ledger.finish(1,a));CHECK(!ledger.finish(1,a));CHECK(ledger.lifetime().lostFrames==2 && ledger.ended().cancelledFrames==4);{Totals next{4,4,0,0,1,1};CHECK(ledger.finish(2,next));}CHECK(ledger.lifetime().lostFrames==2 && ledger.lifetime().cancelledFrames==4 && ledger.ended().lostFrames==0);}
void NonEosLengthIsIgnored(){
    PacketAdmission p;p.reset(3,48000,4,1,4);PacketDescriptor d{};
    CHECK(p.accept(0,false,0xffffffffu,0,100,d)==CommitResult::Accepted);
    CHECK(d.frames==4 && d.firstSourceFrame==0 && d.flags==RawIdentity);
    CHECK(p.accept(1,true,3,4,104,d)==CommitResult::Invalid);
    CHECK(p.accept(1,true,8,4,104,d)==CommitResult::Accepted && d.frames==2);
}
int main(){const Case cases[]={{"PublicationContentionPreservesCommit",PublicationContentionPreservesCommit},{"FullPayloadRecordsKnownLoss",FullPayloadRecordsKnownLoss},{"ConcurrentSnapshotsAreImmutable",ConcurrentSnapshotsAreImmutable},{"UnprovenAllocationIsRefused",UnprovenAllocationIsRefused},{"ValidSnapshotCanBePublishedAfterClockElapsed",ValidSnapshotCanBePublishedAfterClockElapsed},{"IndependentAdmission",IndependentAdmission},{"LifetimeSurvivesAllocation",LifetimeSurvivesAllocation},{"NonEosLengthIsIgnored",NonEosLengthIsIgnored}};return runTests(cases,8);}

