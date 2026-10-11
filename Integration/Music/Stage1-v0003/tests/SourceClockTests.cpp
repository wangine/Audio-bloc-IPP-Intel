#include "SourceDomain.h"
#include "InputFifo.h"
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace entropy::music;
int main(int argc,char** argv) {
    try {
        SourceDomain source;
        EntropyIngressPacketV2 p{};
        p.abiVersion=ENTROPY_PROVIDER_ABI_V2;p.structSize=sizeof(p);
        p.format.structSize=sizeof(p.format);p.format.sampleRate=192000;p.format.channels=2;
        p.format.containerBits=p.format.validBits=64;p.format.blockAlign=16;
        p.format.averageBytesPerSecond=192000*16;p.format.channelMask=3;
        p.format.representation=ENTROPY_REP_F64_PLANAR;
        p.frames=1920;p.payloadBytes=1920*16;p.generation=1;p.qpcFrequency=10000000;
        p.flags=ENTROPY_PACKET_METADATA_PARTIAL;
        std::vector<double> input(1920*2);
        std::fill(input.begin(),input.begin()+1920,.25);std::fill(input.begin()+1920,input.end(),-.125);
        if(!source.prepare(p.format,1,1920))throw std::runtime_error("Prepare failed");
        auto push=[&] {if(!source.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}}))throw std::runtime_error("Packet rejected");};
        const bool legacy=argc==2 && std::string(argv[1])=="--legacy";
        auto pop=[&](StereoBlock& block) {return legacy?source.pop1024(block):source.popClocked1024(block);};
        push();StereoBlock block;
        if(pop(block))throw std::runtime_error("Source began before one capture period plus an output block was available");
        uint64_t nextPacket=3840, validBlocks=0;
        bool begun=false;
        for(uint64_t time=2048;time<192000*20;time+=1024) {
            if(time>=nextPacket) {push();nextPacket+=1920;}
            const bool ready=pop(block);
            if(begun && !ready)throw std::runtime_error("10ms packetization inserted an interior clock gap");
            if(ready) {
                begun=true;validBlocks++;
                for(unsigned i=0;i<1024;i++)if(block.left[i]!=.25 || block.right[i]!=-.125)throw std::runtime_error("Clock alignment changed audio");
            }
        }
        if(!validBlocks || source.telemetry().underruns)throw std::runtime_error("Clocked publication underrun");
        SourceDomain shortStream;
        p.frames=2048;p.payloadBytes=2048*16;
        input.assign(4096,.5);
        if(!shortStream.prepare(p.format,1,2048) || !shortStream.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}}))throw std::runtime_error("Short stream prepare");
        if(shortStream.popClocked1024(block,1,192000))throw std::runtime_error("Short stream skipped startup window");
        if(!shortStream.popClocked1024(block,4096,192000) || !shortStream.popClocked1024(block,5120,192000))throw std::runtime_error("Complete short stream remained stuck in FIFO");
        // A stopped upstream stream is not a RUN underrun. Do not discard
        // queued audio; after it drains, wait and prime again on resumption.
        shortStream.setIngressRunning(false);
        for(uint64_t t=6144;t<192000*3;t+=1024)
            if(shortStream.popClocked1024(block,t,192000))throw std::runtime_error("Idle stream invented PCM");
        if(shortStream.telemetry().underruns)throw std::runtime_error("A legitimate stream pause counted as underrun");
        shortStream.setIngressRunning(true);
        const auto resumeStart=uint64_t(192000*3);
        if(shortStream.popClocked1024(block,resumeStart,192000))throw std::runtime_error("RUN invented PCM");
        if(shortStream.popClocked1024(block,resumeStart+4096,192000) || shortStream.telemetry().underruns!=1)throw std::runtime_error("Empty resumed RUN starvation was hidden");
        // Start a fresh legitimate idle interval for the PCM resumption test.
        shortStream.setIngressRunning(false);
        shortStream.popClocked1024(block,resumeStart+5120,192000);
        const auto resumeUnderruns=shortStream.telemetry().underruns;
        shortStream.setIngressRunning(true);
        if(!shortStream.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}}))throw std::runtime_error("Resume packet rejected");
        if(shortStream.popClocked1024(block,192000*3,192000))throw std::runtime_error("Resume skipped existing prime window");
        if(!shortStream.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}}))throw std::runtime_error("Second resume packet rejected");
        shortStream.setIngressRunning(false);
        for(unsigned i=0;i<4;i++) {
            if(!shortStream.popClocked1024(block,192000*3+1024*(i+1),192000))throw std::runtime_error("Idle transition discarded queued PCM");
            for(unsigned j=0;j<1024;j++)if(block.left[j]!=.5 || block.right[j]!=.5)throw std::runtime_error("Pause changed audio");
        }
        if(shortStream.popClocked1024(block,192000*3+5120,192000) || shortStream.telemetry().underruns!=resumeUnderruns)throw std::runtime_error("Idle drain counted underrun");
        shortStream.setIngressRunning(true);
        for(unsigned i=0;i<2;i++)if(!shortStream.processPacket({p,{reinterpret_cast<const std::byte*>(input.data()),p.payloadBytes}}))throw std::runtime_error("RUN packet rejected");
        for(unsigned i=0;i<4;i++)if(!shortStream.popClocked1024(block,192000*4+1024*i,192000))throw std::runtime_error("RUN failed to prime");
        if(shortStream.popClocked1024(block,192000*4+4096,192000) || shortStream.telemetry().underruns!=resumeUnderruns+1)throw std::runtime_error("Real RUN starvation was hidden");
        InputFifo fenced;
        std::atomic<uint64_t> active{1};
        block.status.generation=1;fenced.push(block,1);
        active=2;block.status.generation=2;fenced.push(block,2);
        if(fenced.currentFill(active,1) || fenced.pop(block,active,1))throw std::runtime_error("Old prime token consumed new generation");
        if(fenced.currentFill(active,2)!=1024 || !fenced.pop(block,active,2) || block.status.generation!=2)throw std::runtime_error("New generation block was lost by old prime decision");
        std::cout<<"PASS 20s deterministic 1920-frame capture /1024-frame publication, exact FP64\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
