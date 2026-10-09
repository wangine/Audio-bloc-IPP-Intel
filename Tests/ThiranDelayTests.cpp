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
    for (auto* id : {"globalDelayMs", "musicGlobalDelayMs", "cinemaGlobalDelayMs"})
        if (engine->getAPVTS().getParameter(id)) {
            std::printf("FAIL retired Global Base control still exposed: %s\n", id); return 1;
        }
    auto legacy = native::XmlNode::parse("<PARAMETERS><PARAM id=\"globalDelayMs\" value=\"20\"/><PARAM id=\"musicGlobalDelayMs\" value=\"7\"/><PARAM id=\"cinemaGlobalDelayMs\" value=\"9\"/><PARAM id=\"musicDelayMs1\" value=\"2\"/><PARAM id=\"cinemaDelayMs1\" value=\"3\"/><MANUAL_SAVE_ALL globalDelayMs=\"20\" musicGlobalDelayMs=\"7\" cinemaGlobalDelayMs=\"9\" note=\"preserve\"/><UNKNOWN note=\"preserve\"><PARAM id=\"vendorSetting\" value=\"5\"/></UNKNOWN></PARAMETERS>");
    native::MemoryBlock saved; native::copyXmlToBinary(*legacy, saved);
    if (!engine->restoreStateInformation(saved.getData(),int(saved.getSize()))) return 1;
    if (std::abs(engine->getAPVTS().getRawParameterValue("musicDelayMs1")->load()-2)>0.00013
        || std::abs(engine->getAPVTS().getRawParameterValue("cinemaDelayMs1")->load()-3)>0.00013) {
        std::puts("FAIL obsolete global delay migrated into channel delay"); return 1;
    }
    engine->getStateInformation(saved);
    auto clean = native::getXmlFromBinary(saved.getData(),int(saved.getSize()));
    if (!clean || clean->serialize().find("GlobalDelayMs")!=std::string::npos
        || clean->serialize().find("globalDelayMs")!=std::string::npos
        || clean->getChildWithName("UNKNOWN").getChildWithName("PARAM").getProperty("id").toString()!=native::String("vendorSetting")
        || float(clean->getChildWithName("UNKNOWN").getChildWithName("PARAM").getProperty("value"))!=5
        || clean->getChildWithName("MANUAL_SAVE_ALL").getProperty("note").toString()!=native::String("preserve")) {
        std::puts("FAIL retired Global Base profile cleanup/metadata preservation"); return 1;
    }
    auto set = [&](const std::string& id, float value) {
        auto* p = engine->getAPVTS().getParameter(id.c_str());
        if (!p) { std::printf("FAIL unknown Crossover control %s\n", id.c_str()); return false; }
        p->setValueNotifyingHost(p->convertTo0to1(value)); return true;
    };
    if (!set("fractionalDelayMode1", 3)) return 1;
    set("lrBandLimit", 1);
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
