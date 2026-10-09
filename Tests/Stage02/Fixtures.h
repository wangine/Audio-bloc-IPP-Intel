#pragma once
#include "Engine/AudioGraph.h"
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
namespace stage02 {
constexpr double rate=192000.0;
using Crossover=entropy::standalone::EntropyCrossoverAudioProcessor;
struct Buffers {
    std::array<std::vector<double>,16> data;
    std::array<double*,16> write{};
    std::array<const double*,16> read{};
    explicit Buffers(unsigned n=1024){for(int c=0;c<16;++c){data[c].resize(n);write[c]=data[c].data();read[c]=write[c];}}
    void zero(unsigned n){for(auto* p:write)std::fill_n(p,n,0.0);}
};
struct Configuration {int cinema=0,mode=0,length=2;bool filtered=true,allPass=false,allLeft=false;};
inline int role(const Configuration& c,int ch){
    if(c.allLeft)return 1;
    constexpr int music[]={1,6,11};constexpr int cinema[]={1,6,11,16,20,21,22,23,24,25};
    return c.cinema?cinema[ch%10]:music[ch%3];
}
inline int inputForRole(const Configuration& c,int r){if(r<=5)return c.cinema?3:0;if(r<=10)return c.cinema?4:1;if(r<=12)return c.cinema?5:2;if(r==16)return 6;if(r>=20)return r-13;throw std::runtime_error("unmapped fixture role");}
template<class Set> void configure(const Configuration& c,Set set){
    auto put=[&](const std::string& id,double v){if(!set(id.c_str(),v))throw std::runtime_error("parameter rejected: "+id);};
    put("cinemaSource",c.cinema);put("lrWays",5);put("centerWays",2);put("lrBandLimit",1);put("cinemaLfeFilterBypass",1);
    for(int ch=0;ch<16;++ch){auto channel=[&](const char* key,double v){put(std::string(key)+std::to_string(ch+1),v);};
        channel("source",c.allLeft?1:(ch%3==0?1:ch%3==1?6:11));channel("cinemaRole",role(Configuration{1,c.mode,c.length,c.filtered,c.allPass,c.allLeft},ch));
        channel("processMode",c.mode);channel("firLength",c.length);channel("filterType",0);channel("hpOn",c.filtered);channel("lpOn",c.filtered);channel("hpHz",100);channel("lpHz",10000);
        channel("allPass",c.allPass);channel("apFreq",1000);channel("apQ",1);channel("musicGainDb",0);channel("cinemaGainDb",0);channel("musicDelayMs",0);channel("cinemaDelayMs",0);channel("mute",0);channel("emergencyMute",0);channel("solo",0);channel("polarity",0);channel("protectorOn",0);
    }
}
inline bool set(Crossover& c,const char* id,double v){auto* p=c.getAPVTS().getParameter(id);if(!p)return false;p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(v)));return true;}
inline void prepareIdentity(entropy::output_matrix::OutputMatrixEngine& m,entropy::headroom::HeadroomEngine& h,unsigned n){
    if(!m.prepare(192000,n)||!h.prepare(192000,n))throw std::runtime_error("identity prepare");
    EntropyOutputMatrixState s{};s.struct_size=sizeof(s);s.generation=1;s.active_source_mask=s.enabled_destination_mask=0xffff;for(unsigned c=0;c<16;++c)s.source_destination_mask[c]=1u<<c;
    EntropyHeadroomState t{};t.struct_size=sizeof(t);t.revision=1;t.group_count=1;t.active_channel_mask=0xffff;t.transition_ms=10;t.groups[0].bounded=1;t.groups[0].struct_size=sizeof(t.groups[0]);
    if(m.setState(s)!=ENTROPY_OK||h.setState(t)!=ENTROPY_OK||h.setMode(ENTROPY_HEADROOM_MODE_BIT_PERFECT)!=ENTROPY_OK)throw std::runtime_error("identity state");
}
inline EntropyAudioBlockF64 block(const Buffers& in,Buffers& out,unsigned n){EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.input_channels=b.output_channels=16;b.frames=n;b.inputs=in.read.data();b.outputs=out.write.data();return b;}
}
