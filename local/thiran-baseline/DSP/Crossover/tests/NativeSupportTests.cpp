#ifdef NDEBUG
#undef NDEBUG
#endif
#include "NativeSupport.h"
#include <cassert>
#include <cmath>
#include <iostream>
#ifdef ENTROPY_TEST_ENGINE
#include "PluginProcessor.h"
#endif
using namespace entropy::native;
int main(){
 double a[4]={1,2,3,4},b[4]={5,6,7,8}; double* p[]={a,b};
 AudioBuffer<double> view(p,2,4); auto lane=view.slice(1,1); lane.setSample(0,2,11); assert(b[2]==11);
 SmoothedValue<double,ValueSmoothingTypes::Linear> ramp; ramp.reset(100,0.04);ramp.setCurrentAndTargetValue(0);ramp.setTargetValue(1);
 assert(ramp.getNextValue()==0.25);ramp.setTargetValue(1);assert(ramp.getNextValue()==0.5);assert(ramp.skip(2)==1);
 AudioParameterFloat trip(ParameterID{"trip",1},"Trip",NormalisableRange<float>(5,1000,1,.4f),50);assert(trip.raw.load()==49.999996185302734375f);
 AudioParameterInt select(ParameterID{"select",1},"Select",1,16,1);select.setValueNotifyingHost(select.convertTo0to1(2.5f));assert(select.raw.load()==2);
 AudioParameterChoice choice(ParameterID{"choice",1},"Choice",{"A","B","C","D","E","F"},2);choice.setValueNotifyingHost(choice.convertTo0to1(.5f));assert(choice.raw.load()==0);
 AudioParameterFloat gain(ParameterID{"gain",1},"Gain",NormalisableRange<float>(-60,12,0.1f),0,"dB");
 gain.setValueNotifyingHost(gain.convertTo0to1(-13.2f));assert(std::abs(gain.raw.load()+13.2f)<1e-4);
 XmlNode root("PARAMETERS");root.setProperty("quoted","x & y < z \"ok\"",nullptr);XmlNode c("PARAM");c.setProperty("id","gain",nullptr);c.setProperty("value",-12.5f,nullptr);root.addChild(c,-1,nullptr);
 MemoryBlock blob;copyXmlToBinary(root,blob);auto parsed=getXmlFromBinary(blob.getData(),(int)blob.getSize());assert(parsed&&parsed->getProperty("quoted").toString()=="x & y < z \"ok\"");
 assert((unsigned char*)blob.getData() && ((unsigned char*)blob.getData())[0]==0x56);assert(!getXmlFromBinary(blob.getData(),9));
 assert(!XmlNode::parse("<PARAMETERS note=\"&#1;\"/>"));assert(!XmlNode::parse(std::string("<PARAMETERS note=\"")+char(0xff)+"\"/>"));
 ParameterStore::ParameterLayout defaults;defaults.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"delay",1},"Delay",NormalisableRange<float>(-800,800,.001f),0));defaults.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"hold",1},"Hold",NormalisableRange<float>(5,1000,1,.4f),50));ParameterStore initial(std::move(defaults));assert(initial.getRawParameterValue("delay")->load()==6.103515625e-05f);assert(initial.getRawParameterValue("hold")->load()==49.999996185302734375f);
 ParameterStore::ParameterLayout layout;layout.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"cutoff",1},"Cutoff",NormalisableRange<float>(2,60000,.1f,.28f),80.f));layout.push_back(std::make_unique<AudioParameterInt>(ParameterID{"selected",1},"Selected",1,16,1));ParameterStore partial(std::move(layout));auto*cutoff=partial.getParameter("cutoff");cutoff->setValueNotifyingHost(cutoff->convertTo0to1(1234.5f));auto replacement=XmlNode::parse("<PARAMETERS metadata=\"preserve\"><PARAM id=\"selected\" value=\"4\"/><UNKNOWN marker=\"test\"/></PARAMETERS>");partial.replaceState(*replacement);assert(cutoff->raw.load()==80);assert(partial.getRawParameterValue("selected")->load()==4);assert(partial.state.getProperty("metadata").toString()=="preserve");assert(partial.state.getChildWithName("UNKNOWN").isValid());bool restoredNode=false;for(int i=0;i<partial.state.getNumChildren();++i){auto n=partial.state.getChild(i);if(n.hasType("PARAM")&&n.getProperty("id").toString()=="cutoff")restoredNode=true;}assert(restoredNode);
 const char bad[]="<PARAMETERS><PARAM id=\"gain\" value=\"nan\"/></PARAMETERS>";assert(!XmlNode::parse("<PARAMETERS><PARAM></PARAMETERS>"));
#ifdef ENTROPY_TEST_ENGINE
 auto engine=std::make_unique<entropy::standalone::EntropyCrossoverAudioProcessor>();
 auto& params=engine->getAPVTS(); auto* hp=params.getParameter("hpHz1");const auto before=hp->raw.load();
 for(const auto* xml:{"<WRONG/>","<PARAMETERS><PARAM id=\"hpHz1\" value=\"nan\"/></PARAMETERS>","<PARAMETERS><PARAM id=\"hpHz1\" value=\"1\"></PARAMETERS>"}){
  auto root=XmlNode::parse(xml);if(root){MemoryBlock bad;copyXmlToBinary(*root,bad);assert(!engine->restoreStateInformation(bad.getData(),(int)bad.getSize()));assert(hp->raw.load()==before);}
 }
 auto legacy=XmlNode::parse("<PARAMETERS extra=\"preserve\"><PARAM id=\"gainDb1\" value=\"-6\"/><PARAM id=\"globalDelayMs\" value=\"3\"/><PARAM id=\"delayMs1\" value=\"2\"/><MANUAL_SAVE_ALL note=\"old\"/><UNKNOWN metadata=\"yes\"/></PARAMETERS>");MemoryBlock saved;copyXmlToBinary(*legacy,saved);
 assert(engine->restoreStateInformation(saved.getData(),(int)saved.getSize()));assert(params.getRawParameterValue("musicGainDb1")->load()==-6);assert(params.getRawParameterValue("cinemaGainDb1")->load()==-6);assert(std::abs(params.getRawParameterValue("musicGlobalDelayMs")->load()-3)<1e-5);assert(std::abs(params.getRawParameterValue("cinemaDelayMs1")->load()-2)<1e-4);
 engine->getStateInformation(saved);auto resaved=getXmlFromBinary(saved.getData(),(int)saved.getSize());assert(resaved->getProperty("extra").toString()=="preserve");assert(resaved->getChildWithName("UNKNOWN").isValid());
 assert(!params.getRawParameterValue("musicTrimDb16"));
 const auto set=[&](const char* id,float value){auto*p=params.getParameter(id);p->setValueNotifyingHost(p->convertTo0to1(value));};
 set("musicGlobalDelayMs",0);set("musicDelayMs1",0);set("musicGainDb1",6);engine->prepareToPlay(48000,64);
 AudioBuffer<double> audio(16,64);MidiBuffer midi;for(int i=0;i<64;++i)audio.setSample(0,i,1.234567891234);engine->processBlock(audio,midi);
 const double expected=1.234567891234*std::pow(10.,6./20.);assert(std::abs(audio.getSample(0,63)-expected)<1e-11);assert(audio.getSample(0,63)>2.);
 engine->prepareToPlay(48000,64);audio.clear();for(int i=0;i<64;++i)audio.setSample(0,i,1.234567891334);engine->processBlock(audio,midi);
 assert(std::abs(audio.getSample(0,63)-expected-1e-10*std::pow(10.,6./20.))<1e-14);
#endif
 std::cout<<"native support tests passed\n";
}
