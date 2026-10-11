#include "TestSupport.h"
#include "../audit/PcmVerifier.h"
#include "../audit/StereoMetrics.h"
#include "../provider/EndpointIdentity.h"
#include "../audit/PauseEvidence.h"
#include "../audit/Fp64Verifier.h"
#include <algorithm>
#include <cmath>
using namespace entropy::music;
using namespace entropy::music::audit;
EntropyIngressPacketV2 metadata(const PcmFormat& f,uint64_t first,uint32_t frames,uint64_t seq){EntropyIngressPacketV2 p{};p.abiVersion=0x20000;p.structSize=160;std::memcpy(&p.format,&f,64);p.generation=1;p.packetSequence=seq;p.firstSourceFrame=first;p.qpcFrequency=10000000;p.frames=frames;p.payloadBytes=frames*f.blockAlign;p.flags=RawIdentity;p.originalContainerBits=f.containerBits;p.originalValidBits=f.validBits;return p;}
std::vector<uint8_t> pattern(const PcmFormat& f,uint64_t first,uint32_t count){std::vector<uint8_t> bytes(count*f.blockAlign);for(uint32_t i=0;i<count;++i)PcmPattern::frame(first+i,f,bytes.data()+i*f.blockAlign);return bytes;}
void VerifierRejectsMissingFirstAndLastFrame(){auto f=format();auto bytes=pattern(f,0,8);PcmVerifier missingFirst(f,8);CHECK(!missingFirst.accept(metadata(f,1,7,1),bytes.data()+4));CHECK(!missingFirst.finish(true,0,0,true).pass);PcmVerifier missingLast(f,8);CHECK(missingLast.accept(metadata(f,0,7,1),bytes.data()));CHECK(!missingLast.finish(true,0,0,true).pass);}
void VerifierRejectsDuplicateAndReorderedFrame(){auto f=format();auto bytes=pattern(f,0,8);PcmVerifier v(f,8);CHECK(v.accept(metadata(f,0,4,1),bytes.data()));CHECK(!v.accept(metadata(f,0,4,2),bytes.data()));PcmVerifier reorder(f,8);CHECK(!reorder.accept(metadata(f,4,4,1),bytes.data()+16));}
void VerifierChecksAllContainerBytes(){for(auto bits:{16u,32u}){auto f=format(192000,bits);auto bytes=pattern(f,0,8);PcmVerifier v(f,8);CHECK(v.accept(metadata(f,0,8,1),bytes.data()));CHECK(v.finish(true,0,0,true).pass);bytes[f.blockAlign*7]^=1;PcmVerifier bad(f,8);CHECK(!bad.accept(metadata(f,0,8,1),bytes.data()));}}
void IncompleteAuditCannotPass(){auto f=format();auto bytes=pattern(f,0,8);for(unsigned error=0;error<4;++error){PcmVerifier v(f,8);CHECK(v.accept(metadata(f,0,8,1),bytes.data()));CHECK(!v.finish(error!=0,error==1?1:0,error==2?1:0,error!=3).pass);}}
void StereoKnownMidSideAndPearson(){StereoMetrics mono,anti,dc,silent;for(int i=0;i<8;++i){double x=(i%2)?0.25:-0.25;mono.add(x,x);anti.add(x,-x);dc.add(x+0.5,x-0.5);silent.add(0,0);}auto m=mono.result(),a=anti.result(),d=dc.result(),s=silent.result();CHECK(m.sideRms==0 && m.pearson && *m.pearson>0.999);CHECK(a.midRms==0 && a.pearson && *a.pearson<-.999);CHECK(d.pearson && *d.pearson>.999 && d.uncentered && *d.uncentered<0);CHECK(!s.pearson && !s.uncentered);}
void HarnessRejectsPhysicalOrForeignEndpoint(){using namespace entropy::music::provider;DeviceIdentity d;d.hardwareId=L"ROOT\\PhysicalDAC";d.service=L"EntropyMusicRenderV2";d.interfacePath=L"foreign";d.windowsEndpointId=L"foreign";CHECK(!validIdentity(d));}
void InterruptedStopIsNotDrainPass(){auto f=format();auto bytes=pattern(f,0,8);PcmVerifier v(f,8);CHECK(v.accept(metadata(f,0,8,1),bytes.data()));CHECK(!v.finish(true,0,0,false).pass);}
void ExpectedFormatAndGenerationAreExternal(){auto expected=format(96000),other=format(48000);auto bytes=pattern(other,0,8);PcmVerifier wrongRate(expected,8);CHECK(!wrongRate.accept(metadata(other,0,8,1),bytes.data()));PcmVerifier wrongGeneration(other,8,9);CHECK(!wrongGeneration.accept(metadata(other,0,8,1),bytes.data()));auto p=metadata(other,0,8,1);p.generation=9;PcmVerifier correct(other,8,9);CHECK(correct.accept(p,bytes.data()));CHECK(correct.finish(true,0,0,true).pass);}
void LossIsCumulative(){auto f=format();auto bytes=pattern(f,0,8);PcmVerifier v(f,8);auto p=metadata(f,0,4,1);p.lostFrames=7;CHECK(!v.accept(p,bytes.data()));p.firstSourceFrame=4;p.packetSequence=2;CHECK(!v.accept(p,bytes.data()+16));CHECK(v.finish(true,0,0,true).lostFrames==7);}
void PauseMustFreezeBothClocksWithoutNewGeneration(){
    PauseEvidence p{123,123,124,124,7,7,1};CHECK(p.valid());
    p.clockAfter++;CHECK(!p.valid());p.clockAfter=p.clockBefore;
    p.sourceAfter++;CHECK(!p.valid());p.sourceAfter=p.sourceBefore;
    p.generationAfter++;CHECK(!p.valid());p.generationAfter=p.generationBefore;
    p.observed=0;CHECK(!p.valid());p.observed=1;
    p.generationBefore=p.generationAfter=0;CHECK(!p.valid());
}
void Fp64ChecksExactValuesFirstAndLastAndFooter(){
    for(auto bits:{16u,32u}){
        auto f=format(192000,bits);double l[1024]{},r[1024]{};uint8_t bytes[8]{};
        for(uint64_t i=0;i<1024;++i){PcmPattern::frame(i,f,bytes);for(unsigned c=0;c<2;++c){double value;if(bits==16){int16_t v;std::memcpy(&v,bytes+c*2,2);value=v/32768.0;}else{int32_t v;std::memcpy(&v,bytes+c*4,4);value=(v>>8)/8388608.0;}(c?r:l)[i]=value;}}
        StageBusBlockHeader h{};h.frames=1024;h.block_sequence=1;h.validity_mask=1;h.source_generation[0]=7;h.source_rate[0]=192000;
        Fp64Verifier good(f,1024,7);CHECK(good.accept(h,l,r));CHECK(good.finish(true,0));
        Fp64Verifier incomplete(f,2048,7);CHECK(incomplete.accept(h,l,r));CHECK(!incomplete.finish(true,0));
        Fp64Verifier noFooter(f,1024,7);CHECK(noFooter.accept(h,l,r));CHECK(!noFooter.finish(false,0));
        Fp64Verifier duplicate(f,2048,7);CHECK(duplicate.accept(h,l,r));CHECK(!duplicate.accept(h,l,r));
        l[1023]=std::nextafter(l[1023],1.0);Fp64Verifier changed(f,1024,7);CHECK(!changed.accept(h,l,r));CHECK(changed.mismatchedSamples()==1);
        std::rotate(l,l+1,l+1024);Fp64Verifier missingFirst(f,1024,7);CHECK(!missingFirst.accept(h,l,r));
    }
    Fp64Verifier needsSrc(format(44100),1024,7);CHECK(!needsSrc.finish(true,0));
}
void Fp64RejectsInsertedInteriorSilence(){
    auto f=format(192000);double l[1024]{},r[1024]{},silence[1024]{};uint8_t bytes[4]{};
    for(unsigned i=0;i<1024;++i){PcmPattern::frame(i,f,bytes);int16_t a,b;std::memcpy(&a,bytes,2);std::memcpy(&b,bytes+2,2);l[i]=a/32768.0;r[i]=b/32768.0;}
    StageBusBlockHeader h{};h.frames=1024;h.block_sequence=1;h.validity_mask=1;h.source_generation[0]=7;h.source_rate[0]=192000;
    Fp64Verifier v(f,2048,7);CHECK(v.accept(h,l,r));h.block_sequence=2;h.first_frame=1024;h.validity_mask=0;h.source_generation[0]=0;h.source_rate[0]=0;
    CHECK(!v.accept(h,silence,silence));CHECK(!v.finish(true,0));
}
int main(){const Case cases[]={{"VerifierRejectsMissingFirstAndLastFrame",VerifierRejectsMissingFirstAndLastFrame},{"VerifierRejectsDuplicateAndReorderedFrame",VerifierRejectsDuplicateAndReorderedFrame},{"VerifierChecksAllContainerBytes",VerifierChecksAllContainerBytes},{"IncompleteAuditCannotPass",IncompleteAuditCannotPass},{"StereoKnownMidSideAndPearson",StereoKnownMidSideAndPearson},{"HarnessRejectsPhysicalOrForeignEndpoint",HarnessRejectsPhysicalOrForeignEndpoint},{"InterruptedStopIsNotDrainPass",InterruptedStopIsNotDrainPass},{"ExpectedFormatAndGenerationAreExternal",ExpectedFormatAndGenerationAreExternal},{"LossIsCumulative",LossIsCumulative},{"PauseMustFreezeBothClocksWithoutNewGeneration",PauseMustFreezeBothClocksWithoutNewGeneration},{"Fp64ChecksExactValuesFirstAndLastAndFooter",Fp64ChecksExactValuesFirstAndLastAndFooter},{"Fp64RejectsInsertedInteriorSilence",Fp64RejectsInsertedInteriorSilence}};return runTests(cases,12);}
