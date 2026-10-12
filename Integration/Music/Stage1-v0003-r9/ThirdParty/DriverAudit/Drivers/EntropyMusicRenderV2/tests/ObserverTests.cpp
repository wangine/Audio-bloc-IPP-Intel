#include "TestSupport.h"
#include "../provider/AuditQueue.h"
using namespace entropy::music::provider;
void ExactReturnedBytes(){AuditQueue q(2,32);EntropyIngressPacketV2 p{};p.frames=2;p.payloadBytes=16;p.packetSequence=9;uint8_t bytes[16]{};for(unsigned i=0;i<16;++i)bytes[i]=static_cast<uint8_t>(i+7);CHECK(q.push(p,bytes));bytes[0]=99;AuditRecord r;CHECK(q.pop(r));CHECK(r.packet.packetSequence==9 && r.bytes[0]==7 && r.bytes[15]==22);CHECK(!q.pop(r));}
void OverflowInvalidatesAudit(){AuditQueue q(1,16);EntropyIngressPacketV2 p{};p.payloadBytes=16;uint8_t bytes[16]{};CHECK(q.push(p,bytes));CHECK(!q.push(p,bytes));CHECK(q.overflow()==1);AuditRecord r;CHECK(q.pop(r));CHECK(q.push(p,bytes));CHECK(q.overflow()==1);p.payloadBytes=17;CHECK(!q.push(p,bytes));CHECK(q.overflow()==2);}
int main(){const Case cases[]={{"ExactReturnedBytes",ExactReturnedBytes},{"OverflowInvalidatesAudit",OverflowInvalidatesAudit}};return runTests(cases,2);}
