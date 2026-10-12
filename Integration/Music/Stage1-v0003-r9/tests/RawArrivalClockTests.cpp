#include "SourceDomain.h"
#include "MultiChannelSrcEngine.h"
#include <fstream>
#include <vector>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
using namespace entropy::music;
int main(int argc,char** argv)try {
    if(argc!=2&&argc!=4)throw std::runtime_error("Expected timing fixture, optional rate and valid bits");
    const auto rate=argc==4?static_cast<uint32_t>(std::stoul(argv[2])):48000u;
    const auto bits=argc==4?static_cast<uint32_t>(std::stoul(argv[3])):16u;
    if((bits!=16&&bits!=24)||rate%100)throw std::runtime_error("Unsupported test format");
    const auto packetFrames=rate/100;
    SourceDomain domain;
    EntropyIngressPacketV2 p{};p.abiVersion=ENTROPY_PROVIDER_ABI_V2;p.structSize=sizeof(p);
    p.format.structSize=sizeof(p.format);p.format.sampleRate=rate;p.format.channels=2;
    p.format.containerBits=bits==16?16:32;p.format.validBits=bits;p.format.blockAlign=p.format.containerBits/4;
    p.format.averageBytesPerSecond=rate*p.format.blockAlign;p.format.channelMask=3;p.format.formatTag=1;
    p.format.representation=ENTROPY_REP_PCM;
    const uint8_t guid[]={1,0,0,0,0,0,16,0,128,0,0,170,0,56,155,113};std::memcpy(p.format.subFormat,guid,16);
    p.frames=packetFrames;p.payloadBytes=packetFrames*p.format.blockAlign;p.generation=1;p.qpcFrequency=10000000;
    p.flags=ENTROPY_PACKET_RAW_IDENTITY;p.firstSourceFrame=packetFrames;
    if(!domain.prepare(p.format,1))throw std::runtime_error("Prepare failed");
    entropy::src::MultiChannelSrcEngine reference;EntropySrcConfig cfg{};
    cfg.struct_size=sizeof(cfg);cfg.engine=ENTROPY_SRC_R8BRAIN_REFERENCE;cfg.source_rate=rate;
    cfg.destination_rate=192000;cfg.channels=2;cfg.maximum_input_frames=65536;
    if(!reference.configure(cfg))throw std::runtime_error("Reference prepare failed");
    std::array<std::vector<double>,2> in,out,golden;
    for(auto& x:in)x.resize(packetFrames);for(auto& x:out)x.resize(reference.config().required_output_capacity);
    std::vector<int16_t> pcm16(packetFrames*2);std::vector<int32_t> pcm32(packetFrames*2);
    std::ifstream file(argv[1]);std::string line;std::getline(file,line);
    uint64_t valid=0,invalid=0,firstBad=0,sampleIndex=0,lastNow=0;
    while(std::getline(file,line)) {
        const auto now=100000000+std::stoull(line.substr(2));
        lastNow=now;
        if(line[0]=='P') {
            ++p.packetSequence;p.qpcTimestamp=now;
            for(unsigned i=0;i<packetFrames;++i)for(unsigned c=0;c<2;++c){
                // Distinct deterministic channels and frame markers, without track PCM.
                const auto n=p.firstSourceFrame+i;
                const auto sample=static_cast<int32_t>((n*(c?113:73)+c*991)%(bits==16?24001:6000001))-(bits==16?12000:3000000);
                if(bits==16){pcm16[2*i+c]=static_cast<int16_t>(sample);in[c][i]=sample/32768.0;}
                else{pcm32[2*i+c]=sample*256;in[c][i]=sample/8388608.0;}
            }
            const double* inputs[]={in[0].data(),in[1].data()};double* outputs[]={out[0].data(),out[1].data()};
            EntropySrcBlockF64 b{};b.struct_size=sizeof(b);b.channels=2;b.input_frames=packetFrames;
            b.output_capacity=static_cast<uint32_t>(out[0].size());b.inputs=inputs;b.outputs=outputs;
            if(reference.process(b)!=ENTROPY_OK)throw std::runtime_error("Reference process failed");
            for(unsigned c=0;c<2;++c)golden[c].insert(golden[c].end(),out[c].begin(),out[c].begin()+b.produced_frames);
            const auto* bytes=bits==16?reinterpret_cast<const std::byte*>(pcm16.data()):reinterpret_cast<const std::byte*>(pcm32.data());
            if(!domain.processPacket({p,{bytes,p.payloadBytes}}))throw std::runtime_error("Packet rejected");
            p.firstSourceFrame+=packetFrames;
        } else if(line[0]=='T') {
            StereoBlock b{};const bool ready=domain.popClocked1024(b,now,p.qpcFrequency);
            if(ready){
                if(sampleIndex+1024>golden[0].size())throw std::runtime_error("Read before reference samples existed");
                if(std::memcmp(b.left.data(),golden[0].data()+sampleIndex,1024*sizeof(double))||
                   std::memcmp(b.right.data(),golden[1].data()+sampleIndex,1024*sizeof(double)))throw std::runtime_error("PCM order or SRC identity changed");
                sampleIndex+=1024;++valid;
            } else if(valid){++invalid;if(!firstBad)firstBad=now;}
        } else throw std::runtime_error("Unknown event");
    }
    std::cout<<"packets="<<p.packetSequence<<" valid="<<valid<<" interior_invalid="<<invalid<<" underruns="<<domain.underrunCount()<<" first_bad_qpc="<<firstBad<<'\n';
    if(!valid||invalid||domain.underrunCount())throw std::runtime_error("Captured Shared cold-start timing inserted an interior clock gap");
    domain.setIngressRunning(false);
    for(unsigned i=0;i<32;++i){
        lastNow+=53334;StereoBlock b{};
        if(!domain.popClocked1024(b,lastNow,p.qpcFrequency))break;
        if(sampleIndex+1024>golden[0].size()||
           std::memcmp(b.left.data(),golden[0].data()+sampleIndex,1024*sizeof(double))||
           std::memcmp(b.right.data(),golden[1].data()+sampleIndex,1024*sizeof(double)))throw std::runtime_error("Idle drain invented or changed PCM");
        sampleIndex+=1024;
    }
    if(sampleIndex!=golden[0].size()/1024*1024||domain.underrunCount())throw std::runtime_error("Idle transition discarded completed PCM or counted underrun");
    domain.setIngressRunning(true);StereoBlock empty{};
    if(domain.popClocked1024(empty,lastNow+53334,p.qpcFrequency))throw std::runtime_error("RUN invented PCM");
    if(domain.popClocked1024(empty,lastNow+p.qpcFrequency,p.qpcFrequency)||domain.underrunCount()!=1)throw std::runtime_error("Real RAW RUN starvation was hidden");
    std::cout<<"PASS recorded ingress timing, PCM"<<bits<<'/'<<rate<<" -> FP64/192, exact stereo SRC samples\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
