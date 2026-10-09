#include "PluginProcessor.h"
#include <string>
#include <memory>
namespace {
struct Baseline {
    entropy::standalone::EntropyCrossoverAudioProcessor engine;
    entropy::native::AudioBuffer<double> audio{16,1024};
    entropy::native::MidiBuffer midi;
};
}
extern "C" {
void* thiranBaselineCreate() {return new Baseline;}
void thiranBaselineDestroy(void* p) {delete static_cast<Baseline*>(p);}
bool thiranBaselineSet(void* p,const char* id,float value) {
    auto& b=*static_cast<Baseline*>(p);auto* parameter=b.engine.getAPVTS().getParameter(id);
    if(!parameter) return false;
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));return true;
}
void thiranBaselinePrepare(void* p) {static_cast<Baseline*>(p)->engine.prepareToPlay(192000,1024);}
void thiranBaselineProcess(void* p,double* const* lanes,int n) {
    auto& b=*static_cast<Baseline*>(p);
    entropy::native::AudioBuffer<double> view(lanes,16,n);
    b.engine.processBlock(view,b.midi);
}
}
