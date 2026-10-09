#include "PluginProcessor.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

// The real Crossover must expose a selectable delay and retain high-frequency
// amplitude. This fails against 0.0.3 (missing control / linear droop).
int main() {
    using namespace entropy;
    auto engine = std::make_unique<standalone::EntropyCrossoverAudioProcessor>();
    auto set = [&](const std::string& id, float value) {
        auto* p = engine->getAPVTS().getParameter(id.c_str());
        if (!p) { std::printf("FAIL unknown Crossover control %s\n", id.c_str()); return false; }
        p->setValueNotifyingHost(p->convertTo0to1(value)); return true;
    };
    if (!set("fractionalDelayMode1", 3)) return 1;
    set("musicGlobalDelayMs", 0); set("lrBandLimit", 1);
    for (int ch=1; ch<=16; ++ch) {
        auto n=std::to_string(ch);
        set("source"+n,1); set("hpOn"+n,0); set("lpOn"+n,0);
        set("allPass"+n,0); set("protectorOn"+n,0);
        set("musicDelayMs"+n,.044f); set("fractionalDelayMode"+n,3);
    }
    engine->prepareToPlay(192000,256);
    native::AudioBuffer<double> audio(16,256); native::MidiBuffer midi;
    double energy=0; unsigned count=0;
    for(int at=0;at<49152;at+=256) {
        audio.clear();
        for(int i=0;i<256;++i) audio.setSample(0,i,.125*std::sin(2*3.14159265358979323846*60000*(at+i)/192000));
        engine->processBlock(audio,midi);
        if(at>=24576) for(int i=0;i<256;++i) {auto y=audio.getSample(0,i);energy+=y*y;++count;}
    }
    double gain=std::sqrt(energy/count)/(.125/std::sqrt(2.));
    std::printf("60kHz Thiran amplitude gain=%.17g dB=%.12g\n",gain,20*std::log10(gain));
    return std::isfinite(gain)&&std::abs(gain-1)<1e-10?0:1;
}
