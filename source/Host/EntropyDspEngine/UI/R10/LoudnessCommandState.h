#pragma once
#include <entropy/loudness_abi.h>
#include <cstring>
namespace entropy::host {
inline bool loudnessSchemaMatches(const EntropyLoudnessParameter* a,unsigned na,const EntropyLoudnessParameter* b,unsigned nb){
    if(na!=nb||na>64)return false;
    for(unsigned i=0;i<na;++i)
        if(std::strncmp(a[i].identifier,b[i].identifier,sizeof(a[i].identifier))||a[i].kind!=b[i].kind||
           a[i].minimum!=b[i].minimum||a[i].maximum!=b[i].maximum||a[i].step!=b[i].step)return false;
    return true;
}
class LoudnessCommandState {
    EntropyLoudnessSettings desired_{};
    uint64_t revision_=0,sentRevision_=0,ackRevision_=0,pending_=0;
    bool rejected_=false;
public:
    void reset(const EntropyLoudnessSettings& value){desired_=value;revision_=sentRevision_=ackRevision_=pending_=0;rejected_=false;}
    void edit(unsigned index,double value){if(index<desired_.parameter_count){desired_.values[index]=value;++revision_;}}
    bool takeNext(EntropyLoudnessSettings& out) const {if(pending_||rejected_||!dirty())return false;out=desired_;return true;}
    void sent(uint64_t sequence){pending_=sequence;sentRevision_=revision_;}
    void acknowledge(uint64_t seq,bool ok,const EntropyLoudnessSettings& accepted){
        if(!pending_||seq!=pending_)return;pending_=0;rejected_=!ok;
        if(ok){ackRevision_=sentRevision_;if(!dirty())desired_=accepted;}
    }
    void retry(){rejected_=false;}
    const EntropyLoudnessSettings& desired()const{return desired_;}
    bool dirty()const{return revision_!=ackRevision_;}
    bool rejected()const{return rejected_;}
};
}
