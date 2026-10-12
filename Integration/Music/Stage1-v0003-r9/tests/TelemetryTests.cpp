#include "SourceDomain.h"
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
using namespace entropy::music;
int main() {
    SourceDomain source;
    EntropyIngressPacketV2 p{};
    p.abiVersion=ENTROPY_PROVIDER_ABI_V2;p.structSize=sizeof(p);
    p.format.structSize=sizeof(p.format);p.format.sampleRate=192000;p.format.channels=2;
    p.format.containerBits=p.format.validBits=64;p.format.blockAlign=16;
    p.format.averageBytesPerSecond=192000*16;p.format.channelMask=3;
    p.format.representation=ENTROPY_REP_F64_PLANAR;
    p.frames=1024;p.payloadBytes=1024*16;p.generation=1;p.qpcFrequency=10000000;
    p.flags=ENTROPY_PACKET_METADATA_PARTIAL;p.originalValidBits=16;p.originalContainerBits=16;
    std::vector<double> input(2048,.125);
    if(!source.prepare(p.format,1,1024))return 2;
    auto push=[&] {++p.packetSequence;p.firstSourceFrame+=1024;return source.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}});};
    if(!push() || source.telemetry().rate!=192000)return 3;
    StereoBlock block;while(source.pop1024(block)){}
    std::atomic<bool> done{false},failed{false};
    std::thread writer([&] {for(unsigned n=0;n<1500;n++) {if(!push()){failed=true;break;}while(source.pop1024(block)){};}done=true;});
    uint64_t reads=0,blank=0,regressed=0,last=0;
    while(!done) {
        auto t=source.telemetry();reads++;
        if(t.rate!=192000 || t.generation!=1 || t.validBits!=16)blank++;
        if(t.framesIn<last)regressed++;
        last=t.framesIn;
    }
    writer.join();
    source.invalidate();
    if(source.telemetry().rate!=0 || source.telemetry().generation!=0)return 4;
    std::cout<<"reads="<<reads<<" blank="<<blank<<" regressed="<<regressed<<'\n';
    if(failed || !reads || blank || regressed)return 1;
    std::cout<<"PASS telemetry collision preserves last coherent metadata and counters; explicit clear remains clear\n";
}
