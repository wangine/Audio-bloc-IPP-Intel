#include "TestSupport.h"
#include "../protocol/PcmBridgeV1.h"
using namespace entropy::music;
bridge::Packet packet(){return {{bridge::Magic,bridge::Version,168,1},format(),{1,1,0,100,10000000,0,2,RawIdentity,0,0},8,0,0};}
void WireRejectsInvalidVersionAndLengths(){
    auto p=packet();CHECK(bridge::validate(p,168,168)==bridge::DecodeResult::Valid);
    p.header.version++;CHECK(bridge::validate(p,168,168)==bridge::DecodeResult::Invalid);p=packet();
    CHECK(bridge::validate(p,159,168)==bridge::DecodeResult::Invalid);CHECK(bridge::validate(p,169,169)==bridge::DecodeResult::Invalid);
    p.payloadBytes=UINT32_MAX;CHECK(bridge::validate(p,UINT32_MAX,UINT32_MAX)==bridge::DecodeResult::Invalid);
    p=packet();p.reserved2=1;CHECK(bridge::validate(p,168,168)==bridge::DecodeResult::Invalid);
}
void ReadTooSmallDoesNotConsume(){
    auto p=packet();CHECK(bridge::validate(p,168,167)==bridge::DecodeResult::BufferTooSmall);
    uint8_t storage[16]{},input[8]={1,2,3,4,5,6,7,8},out[8]{};RingSlot slots[2]{};PcmRing r;
    CHECK(r.init(storage,4,4,slots,2));CHECK(r.push(p.descriptor,input,8));uint32_t copied=999;
    CHECK(!r.copyFront(out,7,copied));CHECK(copied==0);CHECK(r.count()==1);CHECK(out[0]==0);
    CHECK(r.copyFront(out,8,copied));CHECK(copied==8 && !std::memcmp(out,input,8));CHECK(r.count()==1);
    CHECK(r.consume(1));CHECK(r.count()==0);
}
int main(){const Case cases[]={{"WireRejectsInvalidVersionAndLengths",WireRejectsInvalidVersionAndLengths},{"ReadTooSmallDoesNotConsume",ReadTooSmallDoesNotConsume}};return runTests(cases,2);}
