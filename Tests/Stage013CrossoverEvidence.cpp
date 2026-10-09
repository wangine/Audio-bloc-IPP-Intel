// Bounded offline probes. No source transport, sink, IPC, or hardware entry point.
#include "Engine/Control/ProfileStore.h"
#include "PluginProcessor.h"
#include <bit>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

using namespace entropy;
using Processor=standalone::EntropyCrossoverAudioProcessor;
using J=ipc::Value;
constexpr double rate=192000,pi=3.14159265358979323846;
constexpr int frames=1024;
static void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static void set(Processor& engine,const std::string& id,double value){
    auto* parameter=engine.getAPVTS().getParameter(id.c_str());
    require(parameter!=nullptr,"fixture parameter unavailable");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));
}
static std::unique_ptr<Processor> fixture(int extractor){
    auto engine=std::make_unique<Processor>();
    set(*engine,"cinemaSource",0);set(*engine,"lrWays",1);set(*engine,"centerWays",1);
    set(*engine,"bassFeed",0);set(*engine,"bassLinkMode",2);set(*engine,"bassExtractor",extractor);
    for(int n=1;n<=16;++n){
        const auto suffix=std::to_string(n);
        set(*engine,"source"+suffix,n==1?1:n==2?6:n==3?16:n==4?17:0);
        for(auto name:{"hpOn","lpOn","allPass","protectorOn","polarity","mute","emergencyMute","processMode"})set(*engine,std::string(name)+suffix,0);
        set(*engine,"musicGainDb"+suffix,0);set(*engine,"musicDelayMs"+suffix,0);
        set(*engine,"fractionalDelayMode"+suffix,3);
    }
    return engine;
}
static void process(Processor& engine,native::AudioBuffer<double>& audio){native::MidiBuffer midi;engine.processBlock(audio,midi);}
static J delayControl(){
    const auto range=native::nonnegativeDelayRange();
    native::AudioParameterFloat parameter(native::ParameterID{"delay",1},"Delay",range,0.0f);
    require(parameter.raw.load()==0,"default zero delay acquired a fractional offset");
    parameter.setValueNotifyingHost(parameter.convertTo0to1(0));
    require(parameter.raw.load()==0,"explicit zero delay acquired a fractional offset");
    // Independent frozen legacy conversion; positive controls must keep their bits.
    auto snap=[](float value){return std::clamp(-800.0f+.001f*std::floor((value+800.0f)/.001f+.5f),0.0f,800.0f);};
    auto encode=[&](float value){return std::clamp((snap(value)+800.0f)/1600.0f,0.0f,1.0f);};
    auto decode=[&](float value){return snap(std::clamp(-800.0f+1600.0f*value,0.0f,800.0f));};
    J::Array rows;
    for(float value:{.00006103515625f,.001f,.5f,1.32806396484375f,1.5670166015625f,2.75201416015625f,3.39202880859375f,800.0f}){
        const float legacy=decode(encode(decode(encode(value))));
        parameter.setValueNotifyingHost(parameter.convertTo0to1(value));
        require(std::bit_cast<uint32_t>(legacy)==std::bit_cast<uint32_t>(parameter.raw.load()),"positive delay encoding changed");
        rows.emplace_back(J::Object{{"requestedMs",double(value)},{"acceptedMs",double(parameter.raw.load())},{"legacyBitExact",true}});
    }
    return J::Object{{"zeroMs",0},{"positiveControls",rows}};
}
static J fixedMono(){
    // Removing the legacy division breaks L=R unity and R=0 half gain.
    J::Array rows;
    for(int mode:{0,1}){
        auto engine=fixture(mode);engine->prepareToPlay(rate,frames);
        native::AudioBuffer<double> audio(16,frames);
        for(int n=0;n<16;++n){audio.clear();process(*engine,audio);}
        for(int inputCase=0;inputCase<3;++inputCase){
            audio.clear();double error=0;
            for(int f=0;f<frames;++f){const double x=.125*std::sin(.03125*f);audio.setSample(0,f,x);audio.setSample(1,f,inputCase==0?x:inputCase==1?0:-x);}
            process(*engine,audio);
            for(int f=0;f<frames;++f){const double x=.125*std::sin(.03125*f),expected=inputCase==0?x:inputCase==1?.5*x:0;
                error=std::max(error,std::abs(audio.getSample(2,f)-expected));
                require(audio.getSample(3,f)==audio.getSample(2,f),"COMMON EXTRACTOR Scoop differs from Bass");
                require(audio.getSample(0,f)==x,"mono extraction altered direct L");
                require(audio.getSample(1,f)==(inputCase==0?x:inputCase==1?0:-x),"mono extraction altered direct R");
            }
            require(error==0,"legacy fixed mono average identity changed");
            rows.emplace_back(J::Object{{"mode",mode},{"input",inputCase==0?"L=R":inputCase==1?"R=0":"L=-R"},{"maxAbsError",error}});
        }
    }
    return rows;
}
struct ToneMeasure {
    double sine=0,cosine=0,energy=0,peak=0;uint64_t count=0;
    void add(double y,double angle){sine+=y*std::sin(angle);cosine+=y*std::cos(angle);energy+=y*y;peak=std::max(peak,std::abs(y));++count;}
    std::complex<double> transfer(double amplitude)const{return {2*sine/(count*amplitude),2*cosine/(count*amplitude)};}
};
static J pcaRankOne(){
    J::Array rows;
    // Five fixed bank centres only: characterization, not a general project sweep.
    for(double frequency:{32.,50.,78.,122.,190.}){
        auto engine=fixture(3);engine->prepareToPlay(rate,frames);
        native::AudioBuffer<double> audio(16,frames);ToneMeasure measured;double directError=0;
        constexpr int warm=192000,count=192000; // allow estimator/filter histories to settle
        for(int at=0;at<warm+count;at+=frames){
            const int size=std::min(frames,warm+count-at);
            native::AudioBuffer<double> view(audio.getArrayOfWritePointers(),16,size);
            audio.clear();for(int f=0;f<size;++f){const double x=.125*std::sin(2*pi*frequency*(at+f)/rate);audio.setSample(0,f,x);audio.setSample(1,f,x);}
            process(*engine,view);
            for(int f=0;f<size;++f){const double angle=2*pi*frequency*(at+f)/rate,x=.125*std::sin(angle);
                directError=std::max(directError,std::abs(audio.getSample(0,f)-x));directError=std::max(directError,std::abs(audio.getSample(1,f)-x));
                require(std::isfinite(audio.getSample(2,f)),"PCA rank-one nonfinite output");
                if(at+f>=warm)measured.add(audio.getSample(2,f),angle);
            }
        }
        require(directError==0,"PCA processing altered direct L/R");
        const auto h=measured.transfer(.125);require(measured.energy>0,"PCA silent output cannot pass");
        rows.emplace_back(J::Object{{"frequencyHz",frequency},{"gain",std::abs(h)},{"gainDb",20*std::log10(std::abs(h))},{"phaseDegrees",std::arg(h)*180/pi},{"peak",measured.peak},{"directLrMaxAbsError",directError}});
    }
    return rows;
}
static J lr24(){
    // Wrong Q or cascade count must fail the independent LR24 fc oracle.
    auto engine=fixture(0);set(*engine,"source2",1);set(*engine,"filterType1",2);set(*engine,"filterType2",2);
    set(*engine,"lpOn1",1);set(*engine,"lpHz1",1000);set(*engine,"hpOn2",1);set(*engine,"hpHz2",1000);
    engine->prepareToPlay(rate,frames);native::AudioBuffer<double> audio(16,frames);ToneMeasure lp,hp;
    constexpr int warm=49152,count=19200;
    for(int at=0;at<warm+count;at+=frames){const int size=std::min(frames,warm+count-at);native::AudioBuffer<double> view(audio.getArrayOfWritePointers(),16,size);
        audio.clear();for(int f=0;f<size;++f)audio.setSample(0,f,.125*std::sin(2*pi*1000*(at+f)/rate));process(*engine,view);
        for(int f=0;f<size;++f)if(at+f>=warm){const double angle=2*pi*1000*(at+f)/rate;lp.add(audio.getSample(0,f),angle);hp.add(audio.getSample(1,f),angle);}
    }
    const auto low=lp.transfer(.125),high=hp.transfer(.125),sum=low+high;
    require(std::abs(low-std::complex<double>(-.5,0))<2e-9&&std::abs(high-std::complex<double>(-.5,0))<2e-9,"native IIR LR24 fc complex response mismatch");
    require(std::abs(std::abs(sum)-1)<2e-9,"native IIR LR24 sum magnitude mismatch");
    return J::Object{{"frequencyHz",1000},{"lpGainDb",20*std::log10(std::abs(low))},{"hpGainDb",20*std::log10(std::abs(high))},{"sumMagnitude",std::abs(sum)},{"sumPhaseDegrees",std::arg(sum)*180/pi},{"oracleMaxComplexError",std::max(std::abs(low+std::complex<double>(.5,0)),std::abs(high+std::complex<double>(.5,0)))}};
}
static J thiran(){
    using namespace standalone;
    J::Array rows;
    // A hidden common delay to force order 5 breaks these causal short-delay cases.
    const double delays[]={0,1,1.5,2.5,4.5};const int orders[]={0,0,1,3,5};
    for(int i=0;i<5;++i){const auto d=FractionalDelayDesign::compile(3,delays[i]);require(d.order==orders[i],"short delay added a hidden common base/order");rows.emplace_back(J::Object{{"delaySamples",delays[i]},{"effectiveOrder",d.order},{"integerTap",d.integerTap}});}
    std::array<double,64> history{};int position=0,valid=0;
    auto read=[&](int age)->DelaySample{return {age<valid?history[(position-age+64)%64]:0,0};};
    for(double delay:{0.,4.}){
        FractionalDelay filter;filter.request(FractionalDelayDesign::compile(3,delay),7680);position=valid=0;history.fill(0);
        for(int n=0;n<128;++n){history[position]=n%3?-0.0:.125;valid=std::min(64,valid+1);const auto expected=read(int(delay)).value;const auto y=filter.process(read,{}).value;
            require(std::bit_cast<uint64_t>(y)==std::bit_cast<uint64_t>(expected),"zero/integer Thiran exact bypass changed");position=(position+1)%64;}
    }
    FractionalDelay filter;filter.request(FractionalDelayDesign::compile(3,.5),7680);
    for(int n=0;n<8192;++n){history[position]=.1*std::sin(.03125*n);valid=64;filter.process(read,{});position=(position+1)%64;}
    filter.request(FractionalDelayDesign::compile(3,4.5),7680);
    for(int n=0;n<7680;++n){history[position]=.1*std::sin(.03125*(8192+n));require(std::isfinite(filter.process(read,{}).value),"Thiran edit produces NaN/Inf");position=(position+1)%64;require(filter.transitioning()==(n<7679),"Thiran bank transition duration changed");}
    require(filter.effectiveOrder()==5,"Thiran destination bank did not adopt order");
    return J::Object{{"status","PASS"},{"designs",rows},{"integerSignedZeroIdentity",true},{"editTransitionSamples",7680}};
}
static J actualIsolation(const char* actualPath,const char* referencePath){
    control::ProfileStore profiles;const auto original=profiles.load(std::filesystem::u8path(actualPath));const auto reference=profiles.load(std::filesystem::u8path(referencePath));
    auto a=std::make_unique<Processor>(),b=std::make_unique<Processor>();
    require(a->restoreStateInformation(original.crossover.data(),int(original.crossover.size()))&&b->restoreStateInformation(reference.crossover.data(),int(reference.crossover.size())),"real profile restore failed");
    for(int n=1;n<=16;++n)for(auto prefix:{"musicDelayMs","cinemaDelayMs"})require(a->getAPVTS().getRawParameterValue((std::string(prefix)+std::to_string(n)).c_str())->load()>=0,"negative saved channel delay");
    a->prepareToPlay(rate,frames);b->prepareToPlay(rate,frames);
    native::AudioBuffer<double> audioA(16,frames),audioB(16,frames);double directError=0,bassDifference=0;uint64_t compared=0;std::array<double,3> directEnergy{};
    for(int at=0;at<98304;at+=frames){audioA.clear();audioB.clear();for(int f=0;f<frames;++f){const double t=(at+f)/rate;const double inputs[]={.125*std::sin(2*pi*50*t)+.03125*std::sin(2*pi*1700*t),.09375*std::sin(2*pi*78*t)+.025*std::sin(2*pi*2300*t),.0625*std::sin(2*pi*900*t)};for(int ch=0;ch<3;++ch){audioA.setSample(ch,f,inputs[ch]);audioB.setSample(ch,f,inputs[ch]);}}
        process(*a,audioA);process(*b,audioB);
        for(int ch=0;ch<16;++ch){const int role=a->roleForChannel(ch);for(int f=0;f<frames;++f){require(std::isfinite(audioA.getSample(ch,f))&&std::isfinite(audioB.getSample(ch,f)),"actual profile nonfinite output");if(role>=1&&role<=15){const double delta=audioA.getSample(ch,f)-audioB.getSample(ch,f);directError=std::max(directError,std::abs(delta));require(std::bit_cast<uint64_t>(audioA.getSample(ch,f))==std::bit_cast<uint64_t>(audioB.getSample(ch,f)),"adaptive mono extractor changed actual direct L/R/C path");directEnergy[(role-1)/5]+=audioA.getSample(ch,f)*audioA.getSample(ch,f);++compared;}else if(role>=16&&role<=19)bassDifference=std::max(bassDifference,std::abs(audioA.getSample(ch,f)-audioB.getSample(ch,f)));}}
    }
    require(compared>0&&bassDifference>0,"actual isolation comparison is vacuous");
    for(double energy:directEnergy)require(energy>0,"actual direct L/R/C output is silent");
    return J::Object{{"status","PASS"},{"directLrcMaxAbsError",directError},{"directLrcBitExact",true},{"directLrcEnergy",J::Array{directEnergy[0],directEnergy[1],directEnergy[2]}},{"comparedSamples",J::u64(compared)},{"bassScoopMaxDifference",bassDifference},{"actualProtectedFiltersRetained",true}};
}
int main(int argc,char** argv)try{
    require(argc==1||argc==4,"Usage: EntropyStage013Crossover [actual.entropy reference.entropy output.json]");
    J result=J::Object{{"delayControl",delayControl()},{"fixedMono",fixedMono()},{"pcaRankOne",pcaRankOne()},{"lr24",lr24()},{"thiran",thiran()},{"hardwareOpened",false}};
    if(argc==4){result["actualProfileIsolation"]=actualIsolation(argv[1],argv[2]);std::ofstream output(std::filesystem::u8path(argv[3]));output<<ipc::dump(result);require(bool(output),"result write failed");}
    std::cout<<ipc::dump(result)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
