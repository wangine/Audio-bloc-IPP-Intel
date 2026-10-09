#include "PluginProcessor.h"
#include "Stage02/RealtimeAllocationAudit.h"
#include <bit>
#include <complex>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#ifdef ENTROPY_THIRAN_BASELINE
extern "C" {
void* thiranBaselineCreate(); void thiranBaselineDestroy(void*);
bool thiranBaselineSet(void*,const char*,float);
void thiranBaselinePrepare(void*); void thiranBaselineProcess(void*,double* const*,int);
}
#endif
using namespace entropy;
using namespace standalone;
constexpr double pi=3.14159265358979323846,rate=192000;
int failures=0;
void check(bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}}
double phaseError(std::complex<double> h,double f,double d){return std::remainder(std::arg(h)+2*pi*f*d/rate,2*pi)*180/pi;}
struct Ring {
    std::array<double,1024> data{};int at=0,valid=0;
    void reset(){at=valid=0;}
    DelaySample run(FractionalDelay& filter,double input){
        data[at]=input;valid=std::min(1024,valid+1);
        auto read=[&](int age)->DelaySample{return {age<valid?data[(at-age+1024)%1024]:0,0};};
        auto result=filter.process(read,{});at=(at+1)%1024;return result;
    }
};
void isolated(std::ostream& csv){
    {
        FractionalDelay filter;
        filter.request(FractionalDelayDesign::compile(1,0),1);
        filter.request(FractionalDelayDesign::compile(1,100),1);
        auto reader=[](int age)->DelaySample{return age?DelaySample{.25,7}:DelaySample{.5,100};};
        auto result=filter.process(reader,{});
        check(result.value==.25&&result.epoch==7,"final fade sample must exclude departing epoch");
    }
    // Independent analytic transfer-function fixtures for f=0.5. The oracle
    // never calls the production coefficient builder or filter recurrence.
    const double oracle[3][3]={{-.5156988808944,-4.4799696510932,-17.365229564041},
        {-.0022893744201312,-.26573882642304,-4.0542432648814},
        {-.000015514057158813,-.023560496396629,-1.29916193485}};
    for(int mode=1;mode<=3;++mode)for(double delay:{.01,.5,.99,1.5,2.5,4.5,8.5,500.5})
        for(double f:{10.,1000.,20000.,40000.,60000.}){
            FractionalDelay filter;Ring ring;auto design=FractionalDelayDesign::compile(mode,delay);filter.request(design,7680);
            constexpr int warm=49152,count=19200;double s=0,c=0;
            for(int n=0;n<warm+count;++n){double w=2*pi*f*n/rate,y=ring.run(filter,.125*std::sin(w)).value;check(std::isfinite(y),"nonfinite Thiran output");if(n>=warm){s+=y*std::sin(w);c+=y*std::cos(w);}}
            std::complex<double> h{2*s/(count*.125),2*c/(count*.125)};
            double gain=20*std::log10(std::abs(h)),error=phaseError(h,f,delay);
            check(std::abs(std::abs(h)-1)<1e-9,"Thiran steady amplitude");
            if(delay==8.5&&f>=20000){int col=f==20000?0:f==40000?1:2;check(std::abs(error-oracle[mode-1][col])<1e-8,"phase or integer tap latency");std::printf("THIRAN order=%d f=%.0f gain_db=%.12g phase_error_deg=%.12g delay_error_us=%.12g\n",design.order,f,gain,error,-error/(360*f)*1e6);}
            csv<<"helper,"<<mode<<','<<design.order<<','<<delay<<','<<f<<','<<gain<<','<<error<<','<<-error/(360*f)*1e6<<'\n';
        }
    for(int mode=1;mode<=3;++mode){
        FractionalDelay filter;Ring ring;filter.request(FractionalDelayDesign::compile(mode,8.5),7680);double energy=0,dc=0,moment=0;
        for(int n=0;n<16384;++n){double y=ring.run(filter,n==0?1:0).value;energy+=y*y;dc+=y;moment+=n*y;}
        check(std::abs(energy-1)<1e-12&&std::abs(dc-1)<1e-12,"impulse energy / DC unity");check(std::abs(moment/dc-8.5)<1e-10,"DC group delay adds latency");
        std::printf("IMPULSE mode=%d energy=%.17g DC_group_delay=%.17g samples\n",mode,energy,moment/dc);
        for(int n=0;n<2048;++n)ring.run(filter,.37);filter.reset();ring.reset();filter.request(FractionalDelayDesign::compile(mode,8.5),7680);
        for(int n=0;n<2048;++n)check(ring.run(filter,0).value==0,"old history leaked after reset");
        for(int n=0;n<2048;++n)ring.run(filter,.125);
        double previous=.125,maxStep=0,maxPeak=0;
        for(int n=0;n<24576;++n){if(n%128==0)filter.request(FractionalDelayDesign::compile(n/128%3+1,n<8192?20.99:n<16384?21.01:8.5),7680);double y=ring.run(filter,.125).value;check(std::isfinite(y),"nonfinite transition");maxStep=std::max(maxStep,std::abs(y-previous));maxPeak=std::max(maxPeak,std::abs(y));previous=y;}
        check(maxStep<.001&&maxPeak<.13,"DC transition discontinuity");std::printf("TRANSITION mode=%d max_step=%.12g max_peak=%.12g\n",mode,maxStep,maxPeak);
    }
    for(double delay:{0.,1.,8.,512.}){FractionalDelay filter;Ring ring;filter.request(FractionalDelayDesign::compile(3,delay),7680);
        for(int n=0;n<2048;++n){double y=ring.run(filter,n%2?-0.0:.125).value,want=n<int(delay)?0.0:((n-int(delay))%2?-0.0:.125);check(std::bit_cast<uint64_t>(y)==std::bit_cast<uint64_t>(want),"integer signed-zero bit identity");}}
    for(double delay:{.000001,2.000001,4.000001,8.999999}){FractionalDelay filter;Ring ring;filter.request(FractionalDelayDesign::compile(3,delay),7680);for(int n=0;n<100000;++n)check(std::isfinite(ring.run(filter,.125*std::sin(n*.93)).value),"near-integer pole stability");}
}
struct Native {
    std::unique_ptr<EntropyCrossoverAudioProcessor> engine=std::make_unique<EntropyCrossoverAudioProcessor>();
    native::AudioBuffer<double> audio{16,1024};native::MidiBuffer midi;
    void set(const std::string& id,float value){auto* p=engine->getAPVTS().getParameter(id.c_str());check(p!=nullptr,"missing delay/control parameter");if(p)p->setValueNotifyingHost(p->convertTo0to1(value));}
    void process(int n){std::array<double*,16> ptr{};for(int i=0;i<16;++i)ptr[i]=audio.getWritePointer(i);native::AudioBuffer<double> view(ptr.data(),16,n);engine->processBlock(view,midi);}
    void input(int at,int n,int program,double f){audio.clear();for(int i=0;i<n;++i)audio.setSample(program?3:0,i,.125*std::sin(2*pi*f*(at+i)/rate));}
};
template<class Put> void configure(Put put,int program,int mode){
    put("cinemaSource",float(program));put("lrBandLimit",1);
    for(int ch=1;ch<=16;++ch){std::string n=std::to_string(ch);for(auto id:{"source","cinemaRole"})put(std::string(id)+n,1);for(auto id:{"hpOn","lpOn","allPass","protectorOn","musicGainDb","cinemaGainDb"})put(std::string(id)+n,0);for(auto id:{"musicDelayMs","cinemaDelayMs"})put(std::string(id)+n,.044f+ch*.001f);if(mode>=0)put("fractionalDelayMode"+n,float(mode));}
}
void nativeChecks(std::ostream& csv){
    for(int program=0;program<2;++program)for(int mode=1;mode<=3;++mode){
        Native n;configure([&](const std::string& id,float v){n.set(id,v);},program,mode);n.engine->prepareToPlay(rate,1024);
        // Actual ring + new filter, all 16 outputs: DC group-delay moment
        // catches a wrong integer tap or a hidden acoustic latency/gain.
        std::array<double,16> impulseSum{},impulseMoment{},impulseEnergy{};
        for(int at=0;at<16384;at+=1024){n.audio.clear();if(!at)n.audio.setSample(program?3:0,0,1);n.process(1024);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i){double y=n.audio.getSample(ch,i);impulseSum[ch]+=y;impulseMoment[ch]+=(at+i)*y;impulseEnergy[ch]+=y*y;}}
        for(int ch=0;ch<16;++ch){double expected=double(n.engine->getAPVTS().getRawParameterValue(((program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str())->load())*.001*rate;check(std::abs(impulseSum[ch]-1)<1e-12&&std::abs(impulseEnergy[ch]-1)<1e-12,"native impulse unity/energy");check(std::abs(impulseMoment[ch]/impulseSum[ch]-expected)<1e-9,"native integer delay/latency adoption");}
        n.engine->prepareToPlay(rate,1024);
        std::array<double,16> s{},c{};constexpr int warm=49152,length=19200;const int sizes[]={1,7,31,64,127,256,511,1024};int block=0;
        for(int at=0;at<warm+length;){int count=std::min(sizes[block++%8],warm+length-at);if(at<warm)count=std::min(count,warm-at);n.input(at,count,program,60000);{host::allocation_audit::CycleScope scope;n.process(count);}if(at>=warm)for(int ch=0;ch<16;++ch)for(int i=0;i<count;++i){double y=n.audio.getSample(ch,i),w=2*pi*60000*(at+i)/rate;s[ch]+=y*std::sin(w);c[ch]+=y*std::cos(w);}at+=count;}
        for(int ch=0;ch<16;++ch){std::complex<double> h{2*s[ch]/(length*.125),2*c[ch]/(length*.125)};check(std::isfinite(std::abs(h))&&std::abs(std::abs(h)-1)<1e-9,"Crossover MUSIC/CINEMA active output amplitude");auto* raw=n.engine->getAPVTS().getRawParameterValue(((program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str());double delay=double(raw->load())*.001*rate;int order=n.engine->getFractionalDelayStatus(ch)&0xff;check(order==(mode==3?5:mode==2?3:1),"effective order telemetry");csv<<"native_p"<<program<<"_ch"<<ch+1<<','<<mode<<','<<order<<','<<delay<<",60000,"<<20*std::log10(std::abs(h))<<','<<phaseError(h,60000,delay)<<','<<-phaseError(h,60000,delay)/(360*60000)*1e6<<'\n';}
        for(int ch=1;ch<=16;++ch)n.set("mute"+std::to_string(ch),1);n.engine->serviceUpdates();for(int at=0;at<12288;at+=1024){n.input(at,1024,program,60000);n.process(1024);}check(n.engine->getOutputPeak(0)==0,"ordinary mute did not reach silence");
        for(int ch=1;ch<=16;++ch){n.set("mute"+std::to_string(ch),0);n.set("emergencyMute"+std::to_string(ch),1);}n.engine->serviceUpdates();n.input(0,1024,program,60000);n.process(1024);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i)check(n.audio.getSample(ch,i)==0,"emergency mute immediate silence");
        for(int ch=1;ch<=16;++ch)n.set("emergencyMute"+std::to_string(ch),0);n.engine->serviceUpdates();for(int at=0;at<16384;at+=1024){n.audio.clear();n.process(1024);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i)check(n.audio.getSample(ch,i)==0,"stale Thiran history after immediate reactivation");}
        for(int at=0;at<12288;at+=1024){n.input(at,1024,program,60000);n.process(1024);}
        check(n.engine->getOutputPeak(0)>1e-3,"reactivated active output must have signal");
        auto role=program?"cinemaRole":"source";
        for(int ch=1;ch<=16;++ch)n.set(std::string(role)+std::to_string(ch),0);n.engine->serviceUpdates();n.input(0,1024,program,60000);n.process(1024);
        for(int ch=1;ch<=16;++ch)n.set(std::string(role)+std::to_string(ch),1);n.engine->serviceUpdates();for(int at=0;at<24576;at+=1024){n.audio.clear();n.process(1024);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i)check(n.audio.getSample(ch,i)==0,"topology reset/re-enable leaked old delay history");}
        for(int interpolation:{0,3,1,2,0,3}){
            n.set("fractionalDelayMode1",float(interpolation));n.set(program?"cinemaDelayMs1":"musicDelayMs1",interpolation?.071f:.044f);n.engine->serviceUpdates();
            for(int at=0;at<12288;at+=1024){n.input(at,1024,program,1000);{host::allocation_audit::CycleScope scope;n.process(1024);}for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i)check(std::isfinite(n.audio.getSample(ch,i)),"native Linear/Thiran mode change nonfinite");}
            check((n.engine->getFractionalDelayStatus(0)&0xff)==(interpolation==3?5:interpolation==2?3:interpolation==1?1:0),"native mode did not settle");check(n.engine->getOutputPeak(0)>1e-3,"mode switch lost active signal");
        }
        // One FIR8193 lane and 15 IIR bypass lanes, OFFLINE: the unchanged
        // common-latency policy must align every lane with no added Thiran delay.
        n.set("processMode1",1);n.set("firLength1",2);n.engine->prepareToPlay(rate,1024);
        check(n.engine->getReportedFirLatencySamples()==4096,"FIR8193 fixture latency");
        impulseSum.fill(0);impulseMoment.fill(0);impulseEnergy.fill(0);
        for(int at=0;at<32768;at+=1024){n.audio.clear();if(!at)n.audio.setSample(program?3:0,0,1);{host::allocation_audit::CycleScope scope;n.process(1024);}for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i){double y=n.audio.getSample(ch,i);impulseSum[ch]+=y;impulseMoment[ch]+=(at+i)*y;impulseEnergy[ch]+=y*y;}}
        for(int ch=0;ch<16;++ch){double expected=4096+double(n.engine->getAPVTS().getRawParameterValue(((program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str())->load())*.001*rate;check(std::abs(impulseSum[ch]-1)<1e-11&&std::abs(impulseEnergy[ch]-1)<1e-11,"FIR + Thiran impulse energy");check(std::abs(impulseMoment[ch]/impulseSum[ch]-expected)<1e-8,"Thiran FIR compensation changed total latency");}
        // Exercise all three existing output paths during FIR / IIR / AP fades.
        for(int fir:{0,1,0}){n.set("processMode1",float(fir));n.set("allPass1",1);n.engine->serviceUpdates();for(int at=0;at<16384;at+=1024){n.input(at,1024,program,1000);{host::allocation_audit::CycleScope scope;n.process(1024);}for(int i=0;i<1024;++i)check(std::isfinite(n.audio.getSample(0,i)),"FIR/IIR/AP transition nonfinite");}check(n.engine->getOutputPeak(0)>1e-3,"FIR/IIR/AP transition lost signal");}
    }
#ifdef ENTROPY_THIRAN_BASELINE
    uint64_t mismatch=0;double maxAbs=0;
    for(int program=0;program<2;++program){Native n;void* baseline=thiranBaselineCreate();
        // The immutable baseline retains Global Base; compare the common
        // per-channel-only configuration explicitly, with its base disabled.
        check(thiranBaselineSet(baseline,"musicGlobalDelayMs",0)&&thiranBaselineSet(baseline,"cinemaGlobalDelayMs",0),"baseline global base disabled");
        configure([&](const std::string& id,float v){n.set(id,v);check(thiranBaselineSet(baseline,id.c_str(),v),"baseline parameter");},program,-1);n.engine->prepareToPlay(rate,1024);thiranBaselinePrepare(baseline);native::AudioBuffer<double> ref(16,1024);std::array<double*,16> ptr{};for(int i=0;i<16;++i)ptr[i]=ref.getWritePointer(i);
        for(int at=0;at<16384;at+=1024){n.input(at,1024,program,40000);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i)ref.setSample(ch,i,n.audio.getSample(ch,i));n.process(1024);thiranBaselineProcess(baseline,ptr.data(),1024);for(int ch=0;ch<16;++ch)for(int i=0;i<1024;++i){double a=n.audio.getSample(ch,i),b=ref.getSample(ch,i);mismatch+=std::bit_cast<uint64_t>(a)!=std::bit_cast<uint64_t>(b);maxAbs=std::max(maxAbs,std::abs(a-b));}}thiranBaselineDestroy(baseline);}
    std::printf("LINEAR v0.0.3 bit_mismatches=%llu max_abs=%.17g\n",mismatch,maxAbs);check(mismatch==0,"Linear changed vs v0.0.3");
#else
    std::puts("BLOCKED immutable baseline missing");++failures;
#endif
    Native n;n.set("fractionalDelayMode1",3);auto legacy=native::XmlNode::parse("<PARAMETERS><PARAM id=\"globalDelayMs\" value=\"20\"/></PARAMETERS>");native::MemoryBlock blob;native::copyXmlToBinary(*legacy,blob);check(n.engine->restoreStateInformation(blob.getData(),int(blob.getSize())),"legacy profile restore");for(int ch=1;ch<=16;++ch)check(n.engine->getAPVTS().getRawParameterValue(("fractionalDelayMode"+std::to_string(ch)).c_str())->load()==0,"old profile must restore Linear");
    n.engine->prepareToPlay(rate,1024);n.set("fractionalDelayMode1",3);n.engine->serviceUpdates();n.set("cinemaSource",1);n.engine->serviceUpdates();n.set("fractionalDelayMode1",1);n.engine->serviceUpdates();n.set("cinemaSource",0);n.engine->serviceUpdates();check(n.engine->getAPVTS().getRawParameterValue("fractionalDelayMode1")->load()==3,"MUSIC Thiran scene recall");
    n.engine->getStateInformation(blob);Native restored;check(restored.engine->restoreStateInformation(blob.getData(),int(blob.getSize())),"new profile restore");restored.engine->prepareToPlay(rate,1024);restored.set("cinemaSource",1);restored.engine->serviceUpdates();check(restored.engine->getAPVTS().getRawParameterValue("fractionalDelayMode1")->load()==1,"CINEMA Thiran scene round-trip");
}
int main(int argc,char** argv){
    std::filesystem::path path=argc>1?argv[1]:"thiran-results";std::filesystem::create_directories(path);std::ofstream csv(path/"numeric.csv");csv.precision(17);csv<<"path,mode,effective_order,delay_samples,frequency_hz,gain_db,phase_error_deg,phase_delay_error_us\n";
    check(host::allocation_audit::installHooks(),"audit import hooks");host::allocation_audit::reset();isolated(csv);nativeChecks(csv);auto a=host::allocation_audit::snapshot();std::printf("RT new=%llu delete=%llu heapAlloc=%llu heapFree=%llu waits=%llu locks=%llu io=%llu\n",a.cppNew,a.cppDelete,a.heapAlloc,a.heapFree,a.wait,a.lock,a.io);check(!(a.cppNew||a.cppDelete||a.heapAlloc||a.heapFree||a.wait||a.lock||a.io),"Crossover processing realtime audit");std::printf("THIRAN focused regression failures=%d\n",failures);return failures?1:0;
}
