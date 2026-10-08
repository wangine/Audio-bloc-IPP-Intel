#include "Fixtures.h"
#include "ReferenceBridge.h"
#include "Golden.h"
#include "RealtimeAllocationAudit.h"
#include <fstream>
#include <iomanip>
#include <bit>
#include <complex>
#include <cstdio>
#include <numbers>
#include <intrin.h>
#include <memory>
#include <limits>
#include <sstream>
namespace stage02 {
constexpr double pi=std::numbers::pi_v<double>;
namespace audit=entropy::host::allocation_audit;
int failures=0;
int goldenDiagnosticsOutsideTolerance=0;
class ResultCsv : public std::ofstream {
public:
    explicit ResultCsv(const std::string& path):std::ofstream(path) {
        exceptions(std::ios::failbit|std::ios::badbit);
    }
};
void require(bool ok,const char* text){if(!ok){++failures;std::printf("FAIL %s\n",text);}}
struct Sum {double value=0,correction=0;void add(double x){double y=x-correction,t=value+y;correction=(t-value)-y;value=t;}};
struct Fit {
    Sum ss,cc,sc,s,c,y,ys,yc;uint64_t n=0;
    void add(double v,double a,double b){ss.add(a*a);cc.add(b*b);sc.add(a*b);s.add(a);c.add(b);y.add(v);ys.add(v*a);yc.add(v*b);++n;}
    std::complex<double> solve()const {
        if(!n)return {};
        double m[3][4]={{ss.value,sc.value,s.value,ys.value},{sc.value,cc.value,c.value,yc.value},{s.value,c.value,double(n),y.value}};
        for(int j=0;j<3;++j){int k=j;for(int i=j+1;i<3;++i)if(std::abs(m[i][j])>std::abs(m[k][j]))k=i;for(int h=j;h<4;++h)std::swap(m[k][h],m[j][h]);if(std::abs(m[j][j])<1e-15)return {};double d=m[j][j];for(int h=j;h<4;++h)m[j][h]/=d;for(int i=0;i<3;++i)if(i!=j){double f=m[i][j];for(int h=j;h<4;++h)m[i][h]-=f*m[j][h];}}
        return {m[0][3],m[1][3]}; // y=A*sin + B*cos; complex amplitude A+jB
    }
};
struct Metric {
    Sum error2,a2,b2;double maximum=0,peakA=0,peakB=0;uint64_t count=0,bits=0,nonfinite=0;Fit a,b;
    int64_t firstA=-1,firstB=-1,peakAtA=-1,peakAtB=-1;
    void add(double x,double y,uint64_t at,double s,double c,bool fit){
        if(!std::isfinite(x)||!std::isfinite(y)){++nonfinite;return;}
        double e=x-y;maximum=std::max(maximum,std::abs(e));error2.add(e*e);a2.add(x*x);b2.add(y*y);++count;bits+=std::bit_cast<uint64_t>(x)!=std::bit_cast<uint64_t>(y);
        if(std::abs(x)>peakA){peakA=std::abs(x);peakAtA=at;}if(std::abs(y)>peakB){peakB=std::abs(y);peakAtB=at;}
        if(firstA<0&&std::abs(x)>1e-10)firstA=at;if(firstB<0&&std::abs(y)>1e-10)firstB=at;
        if(fit){a.add(x,s,c);b.add(y,s,c);}
    }
    double rmsError()const{return count?std::sqrt(error2.value/count):INFINITY;}
    double rmsA()const{return count?std::sqrt(a2.value/count):0;}double rmsB()const{return count?std::sqrt(b2.value/count):0;}
    bool pass(bool active)const{return count&&!nonfinite&&maximum<=1e-12+1e-11*peakB&&rmsError()<=1e-13+1e-12*rmsB()&&(!active||(peakA>1e-10&&peakB>1e-10));}
};
double wrap(double v){return std::remainder(v,2*pi);}
struct RunPair {
    entropy::engine::AudioGraph graph;
    void* reference=nullptr;
    std::unique_ptr<Golden> golden;
    Buffers in,out,ref,gold;
    explicit RunPair(const Configuration& c,const char* path){
        if(!graph.prepare(1024))throw std::runtime_error("graph prepare");reference=stage02ReferenceCreate(1024);
        // Program adoption restores its saved scene on the control side. Adopt
        // it before editing that scene, matching V07's sequential ABI setters.
        graph.setOfflineParameter("cinemaSource",c.cinema);graph.applyOfflineParameters();
        stage02ReferenceSet(reference,"cinemaSource",c.cinema);stage02ReferenceApply(reference,false);
        configure(c,[&](const char* k,double v){return graph.setOfflineParameter(k,v);});configure(c,[&](const char* k,double v){return stage02ReferenceSet(reference,k,v);});
        if(!graph.applyOfflineParameters(true)||!stage02ReferenceApply(reference,true))throw std::runtime_error("pair config");
        if(path){golden=std::make_unique<Golden>(path);golden->prepare();configure(c,[&](const char* k,double v){return golden->set(k,v);});golden->prepare();}
    }
    ~RunPair(){if(reference)stage02ReferenceDestroy(reference);}
    void process(unsigned n,uint64_t at){if(graph.process(in.read.data(),out.write.data(),n)!=ENTROPY_OK||stage02ReferenceProcess(reference,in.read.data(),ref.write.data(),n)!=ENTROPY_OK)throw std::runtime_error("pair process");if(golden&&golden->process(in,gold,n,at)!=ENTROPY_OK)throw std::runtime_error("V07 process");}
    void update(const char* id,double v){if(!graph.setOfflineParameter(id,v)||!stage02ReferenceSet(reference,id,v))throw std::runtime_error("pair update");if(!graph.applyOfflineParameters()||!stage02ReferenceApply(reference,false))throw std::runtime_error("pair apply");if(golden&&!golden->set(id,v))throw std::runtime_error("gold update");}
};
std::complex<double> analyticTransfer(const Configuration& c,int ch,double f,void* reference){
    auto param=[&](const char* key){return stage02ReferenceGet(reference,(std::string(key)+std::to_string(ch+1)).c_str());};
    double hpHz=param("hpHz"),lpHz=param("lpHz"),apHz=param("apFreq"),apQ=param("apQ");
    int r=role(c,ch);bool bypass=c.cinema&&r==16;std::complex<double> h=1;
    if(!bypass&&c.filtered&&c.mode==0){
        auto z=[&](double fc){return std::complex<double>(0,std::tan(pi*f/rate)/std::tan(pi*fc/rate));};
        auto hp=z(hpHz),lp=z(lpHz);h=(hp*hp)/(hp*hp+std::sqrt(2.0)*hp+1.0)/(lp*lp+std::sqrt(2.0)*lp+1.0);
    }
    int taps=c.length==0?2049:c.length==1?4097:8193;int center=(taps-1)/2,common=c.mode?center:0;
    if(!bypass&&c.mode){
        if(!c.filtered)h=std::polar(1.0,-2*pi*f*center/rate);
        else {h=0;for(int i=0;i<taps;++i){int n=i-center;double ideal=n==0?2*(lpHz-hpHz)/rate:(std::sin(2*pi*lpHz*n/rate)-std::sin(2*pi*hpHz*n/rate))/(pi*n);double w=0.42-0.5*std::cos(2*pi*i/(taps-1))+0.08*std::cos(4*pi*i/(taps-1));h+=ideal*w*std::polar(1.0,-2*pi*f*i/rate);}}
    } else if(bypass&&common)h*=std::polar(1.0,-2*pi*f*common/rate);
    if(!bypass&&c.allPass){auto z=std::complex<double>(0,std::tan(pi*f/rate)/std::tan(pi*apHz/rate));h*=(z*z-z/apQ+1.0)/(z*z+z/apQ+1.0);}
    double delay=std::max(0.0,stage02ReferenceGet(reference,c.cinema?"cinemaGlobalDelayMs":"musicGlobalDelayMs")+param(c.cinema?"cinemaDelayMs":"musicDelayMs"))*rate/1000;
    double whole=std::floor(delay),frac=delay-whole;h*=std::polar(1.0,-2*pi*f*whole/rate)*((1-frac)+frac*std::polar(1.0,-2*pi*f/rate));
    h*=std::pow(10.0,param(c.cinema?"cinemaGainDb":"musicGainDb")/20);
    return h;
}
void writeMetric(std::ostream& csv,const std::string& name,unsigned block,int ch,const char* oracle,const Metric& m,bool active,bool tone,bool impulse,double frequency,std::complex<double> expected={NAN,NAN}){
    auto a=m.a.solve(),b=m.b.solve();double phase=std::abs(b)>1e-10?wrap(std::arg(a)-std::arg(b)):NAN;
    double ampError=tone?std::abs(std::abs(a)-std::abs(expected)):NAN,phaseError=tone&&std::abs(expected)>1e-10?wrap(std::arg(a)-std::arg(expected)):NAN;
    bool analytical=!tone||std::abs(expected)<=1e-10||(ampError<=1e-9+1e-6*std::abs(expected)&&std::abs(phaseError)<=1e-5);
    bool ok=m.pass(active);if(oracle==std::string("native")&&!analytical)ok=false;
    if(!ok){if(std::string(oracle)=="native")++failures;else ++goldenDiagnosticsOutsideTolerance;}
    csv<<name<<','<<block<<','<<ch+1<<','<<oracle<<','<<frequency<<','<<m.maximum<<','<<m.rmsError()<<','<<m.peakA<<','<<m.rmsA()<<','<<m.peakB<<','<<m.rmsB()<<','<<m.bits<<','<<m.nonfinite<<','<<active<<','<<(tone?std::abs(a):NAN)<<','<<(tone?std::abs(b):NAN)<<','<<(tone&&std::abs(a)>1e-10?std::arg(a):NAN)<<','<<(tone&&std::abs(b)>1e-10?std::arg(b):NAN)<<','<<(tone?phase:NAN)<<','<<(impulse?m.firstA:-1)<<','<<(impulse?m.firstB:-1)<<','<<(impulse?m.peakAtA:-1)<<','<<(impulse?m.peakAtB:-1)<<','<<(impulse?m.peakAtA-m.peakAtB:0)<<','<<ampError<<','<<phaseError<<','<<(std::string(oracle)=="V07"?"BLOCKED":ok?"PASS":"FAIL")<<','<<(ok?"WITHIN_NATIVE_TOLERANCE":"OUTSIDE_NATIVE_TOLERANCE")<<'\n';
}
double stimulus(int type,uint64_t i,unsigned ch,double frequency,uint64_t duration,uint32_t& seed){
    double t=i/rate,amp=0.01*(1+0.03*ch),phase=0.1*ch;
    if(type==0)return amp*std::sin(2*pi*frequency*t+phase);
    if(type==1)return i==0?amp:0;
    if(type==2){double a=std::log(60000.0/10.0),seconds=duration/rate;return amp*std::sin(2*pi*10*seconds/a*std::expm1(a*t/seconds)+phase);}
    if(type==3){double x=0;for(double f:{10.0,80.0,1000.0,8000.0,24000.0,60000.0})x+=std::sin(2*pi*f*t+phase);return amp*x/6;}
    seed=1664525u*seed+1013904223u;return amp*(double(seed)/4294967296.0*2-1);
}
int numerical(const char* directory,const char* goldenPath,bool quick,unsigned shard=0,unsigned shards=1,unsigned firstGroup=0){
    ResultCsv parameters(std::string(directory)+"/parameters.csv");parameters<<std::setprecision(17)<<"program,variant,block,output,parameter,native_value,V07_value\n";
    ResultCsv csv(std::string(directory)+"/numerical.csv");csv<<std::setprecision(17)<<"case,block,output,oracle,frequency_hz,max_absolute_error,rms_error,peak,rms,reference_peak,reference_rms,bit_mismatches,nonfinite,expected_active,sine_amplitude,reference_sine_amplitude,phase_rad,reference_phase_rad,phase_difference_rad,first_sample,reference_first_sample,impulse_peak_sample,reference_impulse_peak_sample,peak_lag_samples,analytic_amplitude_error,analytic_phase_error_rad,status,native_tolerance_diagnostic\n";
    const unsigned blocks[]={1,7,31,64,127,256,511,1024};const double frequencies[]={10,20,40,80,160,315,630,1000,2000,4000,8000,16000,24000,40000,60000};
    int cases=0;for(int program=0;program<2;++program)for(int variant=0;variant<7;++variant)for(unsigned size:blocks){
        unsigned blockIndex=0;while(blocks[blockIndex]!=size)++blockIndex;unsigned group=(program*7+variant)*8+blockIndex;
        if(group<firstGroup||group%shards!=shard)continue;
        if(quick&&(size!=256||(variant!=0&&variant!=1&&variant!=4&&variant!=5)))continue;
        Configuration config{program,variant>=2&&variant<=5?1:0,variant==2?0:variant==3?1:2,variant!=0,variant==5||variant==6};
        for(int kind=0;kind<5;++kind)for(int fi=0;fi<(kind==0?(quick?1:15):1);++fi){
            double f=quick?1000:frequencies[fi];unsigned duration=kind==0?std::max(32768u,unsigned(std::ceil(10*rate/f))):kind==2?192000:32768;
            if(quick)duration=32768;RunPair pair(config,goldenPath);
            if(kind==0&&fi==0)for(int ch=0;ch<16;++ch)for(const char* key:{"hpHz","lpHz","apFreq","apQ","musicGainDb","cinemaGainDb","musicDelayMs","cinemaDelayMs","filterType","processMode"}){std::string id=std::string(key)+std::to_string(ch+1);parameters<<program<<','<<variant<<','<<size<<','<<ch+1<<','<<id<<','<<stage02ReferenceGet(pair.reference,id.c_str())<<','<<(pair.golden?pair.golden->get(id.c_str()):NAN)<<'\n';}
            std::array<uint32_t,13> seeds{};for(int ch=0;ch<13;++ch)seeds[ch]=12345+ch*97;
            unsigned pre=16384;uint64_t total=uint64_t(pre)+duration+(kind==1?8193:0);std::array<Metric,16> native{},gold{};
            for(uint64_t at=0;at<total;){unsigned n=unsigned(std::min<uint64_t>(size,total-at));if(at<pre)n=unsigned(std::min<uint64_t>(n,pre-at));
                for(unsigned ch=0;ch<13;++ch)for(unsigned j=0;j<n;++j){bool before=at+j<pre;uint64_t pos=before?at+j:at+j-pre;pair.in.write[ch][j]=kind==0?0.01*(1+0.03*ch)*std::sin(2*pi*f*(int64_t(at+j)-pre)/rate+0.1*ch):(before&&kind==1)?0:stimulus(kind,pos,ch,f,duration,seeds[ch]);}
                pair.process(n,at);if(at>=pre)for(unsigned j=0;j<n;++j){uint64_t pos=at+j-pre;double s=kind==0?std::sin(2*pi*f*pos/rate):0,c=kind==0?std::cos(2*pi*f*pos/rate):0;for(int ch=0;ch<16;++ch){native[ch].add(pair.out.write[ch][j],pair.ref.write[ch][j],pos,s,c,kind==0);if(pair.golden)gold[ch].add(pair.out.write[ch][j],pair.gold.write[ch][j],pos,s,c,kind==0);}}at+=n;
            }
            if(pair.graph.telemetry().firLatencySamples!=stage02ReferenceLatency(pair.reference))++failures;
            if(pair.golden&&pair.graph.telemetry().firLatencySamples!=pair.golden->latency())++goldenDiagnosticsOutsideTolerance;
            const char* modes[]={"unity","IIR-BW12","FIR2049","FIR4097","FIR8193","FIR8193-AP","IIR-AP"};const char* names[]={"sine","impulse","sweep","multitone","noise"};
            std::string name=std::string(program?"CINEMA-":"MUSIC-")+modes[variant]+"-"+names[kind];
            for(int ch=0;ch<16;++ch){bool active=kind!=0||!config.filtered||f>=315&&f<=8000;int source=inputForRole(config,role(config,ch));auto expected=std::polar(0.01*(1+0.03*source),0.1*source)*analyticTransfer(config,ch,f,pair.reference);
                writeMetric(csv,name,size,ch,"native",native[ch],active,kind==0,kind==1,kind==0?f:0,expected);
                if(pair.golden)writeMetric(csv,name,size,ch,"V07",gold[ch],active,kind==0,kind==1,kind==0?f:0,expected);
            }
            ++cases;
        }
        std::printf("NUMERICAL program=%d variant=%d block=%u cases=%d failures=%d\n",program,variant,size,cases,failures);std::fflush(stdout);csv.flush();parameters.flush();
    }
    std::printf("NUMERICAL RESULT cases=%d failures=%d V07=%s V07_outside_native_tolerance=%d (golden acceptance BLOCKED: no published threshold for expanded corpus)\n",cases,failures,goldenPath?"EXECUTED":"BLOCKED",goldenDiagnosticsOutsideTolerance);if(!cases)return 2;return failures?1:0;
}
int identity(const char* directory){
    ResultCsv csv(std::string(directory)+"/identity.csv");csv<<"module,frames,bit_mismatches,status\n";entropy::output_matrix::OutputMatrixEngine matrix;entropy::headroom::HeadroomEngine headroom;prepareIdentity(matrix,headroom,1024);Buffers in,middle,out;
    double values[]={0.0,-0.0,0.25,-0.25,1.0,-1.0,2.0,-2.0,1e-200,-1e-200,1e200,-1e200};
    for(unsigned n:{1u,7u,31u,64u,127u,256u,511u,1024u}){
        for(int ch=0;ch<16;++ch)for(unsigned i=0;i<n;++i)in.write[ch][i]=values[(i+ch)%12];
        auto b=block(in,middle,n);require(matrix.process(b)==ENTROPY_OK,"matrix identity process");b=block(in,out,n);require(headroom.process(b)==ENTROPY_OK,"independent headroom identity process");
        uint64_t bm=0,bh=0;for(int ch=0;ch<16;++ch)for(unsigned i=0;i<n;++i){bm+=std::bit_cast<uint64_t>(in.write[ch][i])!=std::bit_cast<uint64_t>(middle.write[ch][i]);bh+=std::bit_cast<uint64_t>(in.write[ch][i])!=std::bit_cast<uint64_t>(out.write[ch][i]);}
        csv<<"Matrix,"<<n<<','<<bm<<','<<(bm?"FAIL":"PASS")<<'\n'<<"Headroom-independent,"<<n<<','<<bh<<','<<(bh?"FAIL":"PASS")<<'\n';require(!bm,"matrix bit identity including signed zero");require(!bh,"headroom bit identity including signed zero");
        b=block(middle,out,n);require(headroom.process(b)==ENTROPY_OK,"chained headroom identity process");uint64_t chain=0;for(int ch=0;ch<16;++ch)for(unsigned i=0;i<n;++i)chain+=std::bit_cast<uint64_t>(middle.write[ch][i])!=std::bit_cast<uint64_t>(out.write[ch][i]);csv<<"Headroom-after-Matrix,"<<n<<','<<chain<<','<<(chain?"FAIL":"PASS")<<'\n';require(!chain,"chained headroom identity");
    }
    Metric muted;for(int i=0;i<256;++i)muted.add(0,0,i,0,0,false);require(!muted.pass(true),"oracle rejects all-zero active output");
    Metric corrupt;for(int i=0;i<256;++i)corrupt.add(i==9?0.1:0.01,0.01,i,0,0,false);require(!corrupt.pass(true),"oracle rejects corrupt output");
    {std::ostringstream diagnostics;int before=failures,observed=goldenDiagnosticsOutsideTolerance;writeMetric(diagnostics,"diagnostic-self-test",256,0,"V07",corrupt,true,false,false,0);require(failures==before&&goldenDiagnosticsOutsideTolerance==observed+1,"V07 diagnostic does not change native acceptance");}
    Fit known;for(int i=0;i<10000;++i){double a=2*pi*1000*i/rate;known.add(0.25*std::sin(a+0.3),std::sin(a),std::cos(a));}require(std::abs(std::abs(known.solve())-0.25)<1e-13&&std::abs(std::arg(known.solve())-0.3)<1e-13,"amplitude/phase estimator known fixture");
    csv.flush();std::printf("IDENTITY RESULT failures=%d\n",failures);return failures?1:0;
}
int transitions(const char* directory,const char* goldenPath){
    ResultCsv windows(std::string(directory)+"/transition-windows.csv");windows<<"program,block,event,output,late_window_peak,expected_active,status\n";
    ResultCsv csv(std::string(directory)+"/transitions.csv");csv<<std::setprecision(17)<<"case,block,output,oracle,frequency_hz,max_absolute_error,rms_error,peak,rms,reference_peak,reference_rms,bit_mismatches,nonfinite,expected_active,sine_amplitude,reference_sine_amplitude,phase_rad,reference_phase_rad,phase_difference_rad,first_sample,reference_first_sample,impulse_peak_sample,reference_impulse_peak_sample,peak_lag_samples,analytic_amplitude_error,analytic_phase_error_rad,status,native_tolerance_diagnostic\n";
    for(int program=0;program<2;++program)for(unsigned size:{1u,7u,31u,64u,127u,256u,511u,1024u}){
        Configuration c{program,0,2,false,false};RunPair p(c,goldenPath);std::array<Metric,16> native{},gold{};uint32_t seed=12345;std::array<std::array<double,16>,16> latePeak{};std::array<std::array<bool,16>,16> expectedActive{};
        constexpr unsigned spacing=16384,total=spacing*16;
        unsigned segment=0;for(unsigned at=0;at<total;){
            unsigned event=at/spacing;if(at%spacing==0&&event){
                auto all=[&](const char* key,double v){for(int ch=0;ch<16;++ch)p.update((std::string(key)+std::to_string(ch+1)).c_str(),v);};
                switch(event){case 1:all(program?"cinemaGainDb":"musicGainDb",-6);break;case 2:all(program?"cinemaGainDb":"musicGainDb",6);break;case 3:all(program?"cinemaGainDb":"musicGainDb",0);all(program?"cinemaDelayMs":"musicDelayMs",1);break;case 4:all(program?"cinemaDelayMs":"musicDelayMs",1.003);break;case 5:all("mute",1);break;case 6:all("mute",0);break;case 7:all("processMode",1);break;case 8:all("firLength",0);break;case 9:all("firLength",2);all("hpOn",1);all("lpOn",1);break;case 10:all("allPass",1);break;case 11:all("processMode",0);break;case 12:all("emergencyMute",1);break;case 13:all("emergencyMute",0);break;case 14:p.update("source1",0);p.update("cinemaRole1",0);break;case 15:p.update("cinemaSource",1-program);break;}
                segment=event;
            }
            if(segment==9&&at%spacing==4096)for(int ch=0;ch<16;++ch)p.update(("lpHz"+std::to_string(ch+1)).c_str(),12000);
            unsigned n=std::min(size,std::min(total-at,spacing-at%spacing));if(segment==9&&at%spacing<4096)n=std::min(n,4096-at%spacing);for(unsigned ch=0;ch<13;++ch)for(unsigned j=0;j<n;++j)p.in.write[ch][j]=stimulus(3,at+j,ch,1000,total,seed);
            p.process(n,at);for(unsigned j=0;j<n;++j)for(int ch=0;ch<16;++ch){native[ch].add(p.out.write[ch][j],p.ref.write[ch][j],at+j,0,0,false);if(p.golden)gold[ch].add(p.out.write[ch][j],p.gold.write[ch][j],at+j,0,0,false);}
            for(unsigned j=0;j<n;++j)if((at+j)%spacing>=12288)for(int ch=0;ch<16;++ch)latePeak[segment][ch]=std::max(latePeak[segment][ch],std::abs(p.out.write[ch][j]));
            if((segment==5||segment==12)&&at%spacing>=8192)for(int ch=0;ch<16;++ch)for(unsigned j=0;j<n;++j)require(p.out.write[ch][j]==0,"mute reaches exact silence");at+=n;
            if(at%spacing==0){auto get=[&](const char* id){return stage02ReferenceGet(p.reference,id);};bool cinema=get("cinemaSource")>=0.5;for(int ch=0;ch<16;++ch){auto channel=[&](const char* key){return get((std::string(key)+std::to_string(ch+1)).c_str());};int r=int(std::lround(channel(cinema?"cinemaRole":"source")));bool enabled=r>=1&&r<=25;if(r<=5)enabled=r>=1&&r<=get("lrWays");else if(r<=10)enabled=r-5<=get("lrWays");else if(r<=15)enabled=r-10<=get("centerWays");else if(r==18)enabled=get("subSpeakers")>=2;else if(r==19)enabled=get("scoopSpeakers")>=2;expectedActive[segment][ch]=enabled&&channel("mute")<0.5&&channel("emergencyMute")<0.5;}}
        }
        std::string name=program?"CINEMA-events":"MUSIC-events";for(int ch=0;ch<16;++ch){writeMetric(csv,name,size,ch,"native",native[ch],true,false,false,0);if(p.golden)writeMetric(csv,name,size,ch,"V07",gold[ch],true,false,false,0);}
        for(int event=0;event<16;++event)for(int ch=0;ch<16;++ch){bool active=expectedActive[event][ch];bool ok=active?latePeak[event][ch]>1e-8:latePeak[event][ch]==0;if(!ok)++failures;windows<<program<<','<<size<<','<<event<<','<<ch+1<<','<<latePeak[event][ch]<<','<<active<<','<<(ok?"PASS":"FAIL")<<'\n';}
        windows.flush();csv.flush();std::printf("TRANSITION program=%d block=%u failures=%d V07_outside_native_tolerance=%d\n",program,size,failures,goldenDiagnosticsOutsideTolerance);std::fflush(stdout);
    }return failures?1:0;
}
int controls(const char* directory){
    ResultCsv csv(std::string(directory)+"/controls.csv");csv<<std::setprecision(17)<<"program,block,output,gain_db,requested_delay_ms,accepted_delay_ms,frequency_hz,amplitude,expected_amplitude,phase_rad,expected_phase_rad,amplitude_error,phase_error_rad,status\n";
    ResultCsv pulses(std::string(directory)+"/delay-impulses.csv");pulses<<std::setprecision(17)<<"program,block,output,requested_delay_ms,accepted_delay_samples,peak_sample,expected_peak_sample,max_error,status\n";
    for(int program=0;program<2;++program)for(unsigned size:{1u,7u,31u,64u,127u,256u,511u,1024u})for(double delay:{0.0,1.0,1.003}){
        Configuration c{program,0,2,false,false,true};RunPair p(c,nullptr);
        for(int ch=0;ch<16;++ch)p.update((std::string(program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str(),delay);
        p.graph.applyOfflineParameters(true);stage02ReferenceApply(p.reference,true);
        double accepted=stage02ReferenceGet(p.reference,program?"cinemaDelayMs1":"musicDelayMs1")+stage02ReferenceGet(p.reference,program?"cinemaGlobalDelayMs":"musicGlobalDelayMs");
        double samples=std::max(0.0,accepted*rate/1000),fraction=samples-std::floor(samples);unsigned whole=unsigned(std::floor(samples));std::array<Metric,16> impulse{};
        for(unsigned at=0;at<2048;){unsigned n=std::min(size,2048-at);p.in.zero(n);for(unsigned j=0;j<n;++j)if(at+j==0)p.in.write[program?3:0][j]=0.01;p.process(n,at);
            for(unsigned j=0;j<n;++j){double want=(at+j==whole?0.01*(1-fraction):0)+(at+j==whole+1?0.01*fraction:0);for(int ch=0;ch<16;++ch)impulse[ch].add(p.out.write[ch][j],want,at+j,0,0,false);}at+=n;}
        for(int ch=0;ch<16;++ch){auto& m=impulse[ch];bool ok=m.maximum<1e-12&&m.peakA>0.005&&m.peakAtA==int64_t(whole+(fraction>0.5));if(!ok)++failures;pulses<<program<<','<<size<<','<<ch+1<<','<<delay<<','<<samples<<','<<m.peakAtA<<','<<whole+(fraction>0.5)<<','<<m.maximum<<','<<(ok?"PASS":"FAIL")<<'\n';}
        for(double gain:{-6.0,0.0,6.0})for(double f:{10.0,1000.0,60000.0}){
            for(int ch=0;ch<16;++ch)p.update((std::string(program?"cinemaGainDb":"musicGainDb")+std::to_string(ch+1)).c_str(),gain);
            p.graph.applyOfflineParameters(true);stage02ReferenceApply(p.reference,true);std::array<Fit,16> fit{};unsigned pre=16384,count=std::max(32768u,unsigned(std::ceil(10*rate/f)));
            for(unsigned at=0;at<pre+count;){unsigned n=std::min(size,pre+count-at);if(at<pre)n=std::min(n,pre-at);p.in.zero(n);for(unsigned j=0;j<n;++j)p.in.write[program?3:0][j]=0.01*std::sin(2*pi*f*(int64_t(at+j)-pre)/rate);p.process(n,at);
                if(at>=pre)for(unsigned j=0;j<n;++j){double a=2*pi*f*(at+j-pre)/rate;for(int ch=0;ch<16;++ch)fit[ch].add(p.out.write[ch][j],std::sin(a),std::cos(a));}at+=n;}
            for(int ch=0;ch<16;++ch){auto a=fit[ch].solve(),want=0.01*analyticTransfer(c,ch,f,p.reference);double ae=std::abs(std::abs(a)-std::abs(want)),pe=wrap(std::arg(a)-std::arg(want));bool ok=ae<1e-9+1e-6*std::abs(want)&&std::abs(pe)<1e-5;if(!ok)++failures;csv<<program<<','<<size<<','<<ch+1<<','<<gain<<','<<delay<<','<<accepted<<','<<f<<','<<std::abs(a)<<','<<std::abs(want)<<','<<std::arg(a)<<','<<std::arg(want)<<','<<ae<<','<<pe<<','<<(ok?"PASS":"FAIL")<<'\n';}
        }
        // Reject bad input before changing graph history or telemetry.
        auto before=p.graph.telemetry().completedBlocks;p.in.zero(1);p.in.write[0][0]=NAN;require(p.graph.process(p.in.read.data(),p.out.write.data(),1)==ENTROPY_ERROR_PROCESSING&&p.graph.telemetry().completedBlocks==before,"NaN input rejected without advancing graph");
        p.in.write[0][0]=INFINITY;require(p.graph.process(p.in.read.data(),p.out.write.data(),1)==ENTROPY_ERROR_PROCESSING,"Inf input rejected");
        std::printf("CONTROLS program=%d block=%u delay=%.3f accepted=%.9f ms failures=%d\n",program,size,delay,accepted,failures);std::fflush(stdout);csv.flush();pulses.flush();
    }return failures?1:0;
}
int realtime(const char* directory){
    bool hooks=audit::selfTest();require(hooks,"allocation/import audit self-test");
    ResultCsv csv(std::string(directory)+"/realtime.csv");csv<<"case,block,callbacks,cpp_new,cpp_delete,heap_alloc,heap_free,wait,lock,io,status\n";
    for(int program=0;program<2;++program)for(unsigned n:{1u,7u,31u,64u,127u,128u,256u,511u,512u,1024u}){
        Buffers in,out;uint32_t seed=12345;
        for(int caseIndex=0;caseIndex<12;++caseIndex){
            const char* names[]={"first-steady-FIR8193-AP","gain-delay","mute","unmute","FIR8193-to-2049","FIR2049-to-8193","FIR-to-IIR","IIR-to-FIR","program-adoption","FIR-emergency-mute","FIR-emergency-unmute","FIR-topology-disable"};
            Configuration c{program,caseIndex==7?0:1,caseIndex==5?0:2,true,true,true};entropy::engine::AudioGraph g;require(g.prepare(1024),"realtime prepare");
            if(caseIndex==8){for(int scene=0;scene<2;++scene){g.setOfflineParameter("cinemaSource",scene);g.applyOfflineParameters();auto sceneConfig=c;sceneConfig.cinema=scene;configure(sceneConfig,[&](const char* k,double v){return g.setOfflineParameter(k,v);});g.applyOfflineParameters();}}
            g.setOfflineParameter("cinemaSource",program);g.applyOfflineParameters();configure(c,[&](const char* k,double v){return g.setOfflineParameter(k,v);});
            if(caseIndex==3||caseIndex==10)for(int ch=0;ch<16;++ch)g.setOfflineParameter((std::string(caseIndex==3?"mute":"emergencyMute")+std::to_string(ch+1)).c_str(),1);
            g.applyOfflineParameters(true);
            if(caseIndex==10){in.zero(n);require(g.process(in.read.data(),out.write.data(),n)==ENTROPY_OK,"prime emergency-silenced state");}
            if(caseIndex==8){g.setOfflineParameter("cinemaSource",1-program);g.applyOfflineParameters();}
            else if(caseIndex){for(int ch=0;ch<16;++ch){const char* key=caseIndex==1?(program?"cinemaGainDb":"musicGainDb"):caseIndex==2||caseIndex==3?"mute":caseIndex==4||caseIndex==5?"firLength":caseIndex==6||caseIndex==7?"processMode":caseIndex==9||caseIndex==10?"emergencyMute":program?"cinemaRole":"source";double value=caseIndex==1?-6:caseIndex==2||caseIndex==7||caseIndex==9?1:caseIndex==5?2:0;require(g.setOfflineParameter((std::string(key)+std::to_string(ch+1)).c_str(),value),"audit parameter");if(caseIndex==1)g.setOfflineParameter((std::string(program?"cinemaDelayMs":"musicDelayMs")+std::to_string(ch+1)).c_str(),1.003);}g.applyOfflineParameters();}
            const unsigned callbacks=std::max(200u,(16384+n-1)/n);
            audit::reset();for(unsigned i=0;i<callbacks;++i){for(int ch=0;ch<13;++ch)for(unsigned j=0;j<n;++j)in.write[ch][j]=stimulus(4,i*n+j,ch,0,0,seed);EntropyResult r;{audit::CycleScope scope;audit::audioPhase();r=g.process(in.read.data(),out.write.data(),n);}require(r==ENTROPY_OK,"audited graph process");for(int ch=0;ch<16;++ch)for(unsigned j=0;j<n;++j)require(std::isfinite(out.write[ch][j]),"audited finite output");}
            auto s=audit::snapshot();bool ok=!s.cppNew&&!s.cppDelete&&!s.heapAlloc&&!s.heapFree&&!s.wait&&!s.lock&&!s.io;csv<<program<<'-'<<names[caseIndex]<<','<<n<<','<<callbacks<<','<<s.cppNew<<','<<s.cppDelete<<','<<s.heapAlloc<<','<<s.heapFree<<','<<s.wait<<','<<s.lock<<','<<s.io<<','<<(ok?"PASS":"FAIL")<<'\n';require(ok,"callback dynamic allocation/operation audit");
        }
    }
    csv.flush();std::printf("REALTIME dynamic audit failures=%d; bulk resets remain source-level FAIL; unlisted/dynamic APIs outside hook coverage\n",failures);return failures?1:0;
}
int benchmark(const char* directory,unsigned firstRow=0){
    LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);auto now=[](){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;};
    ResultCsv csv(std::string(directory)+"/benchmark.csv");csv<<std::setprecision(12)<<"module,program,configuration,frames,count,median_us,p99_us,max_us,budget_us,max_utilization,status\n";
    std::vector<double> timer(10000);for(auto& t:timer){auto start=now();t=double(now()-start)*1e6/freq.QuadPart;}std::sort(timer.begin(),timer.end());std::printf("QPC frequency=%lld timer_median_us=%.6g timer_p99_us=%.6g\n",freq.QuadPart,timer[5000],timer[9899]);
    unsigned rowIndex=0,executedRows=0;for(int program=0;program<2;++program)for(unsigned n:{128u,256u,512u,1024u})for(int variant=0;variant<10;++variant){
        Configuration c{program,variant>=2?1:0,2,variant!=0,variant==1||variant==3,true};
        auto cross=std::make_unique<Crossover>();cross->prepareToPlay(rate,1024);set(*cross,"cinemaSource",program);cross->serviceUpdates();configure(c,[&](const char* k,double v){return set(*cross,k,v);});cross->prepareToPlay(rate,1024);
        entropy::engine::AudioGraph graph;require(graph.prepare(1024),"bench graph prepare");graph.setOfflineParameter("cinemaSource",program);graph.applyOfflineParameters();configure(c,[&](const char* k,double v){return graph.setOfflineParameter(k,v);});graph.applyOfflineParameters(true);
        if(variant==9){for(int scene=0;scene<2;++scene){auto other=c;other.cinema=scene;set(*cross,"cinemaSource",scene);cross->serviceUpdates();configure(other,[&](const char* k,double v){return set(*cross,k,v);});cross->serviceUpdates();graph.setOfflineParameter("cinemaSource",scene);graph.applyOfflineParameters();configure(other,[&](const char* k,double v){return graph.setOfflineParameter(k,v);});graph.applyOfflineParameters();}set(*cross,"cinemaSource",program);cross->serviceUpdates();cross->prepareToPlay(rate,1024);graph.setOfflineParameter("cinemaSource",program);graph.applyOfflineParameters(true);}
        entropy::output_matrix::OutputMatrixEngine m;entropy::headroom::HeadroomEngine h;prepareIdentity(m,h,1024);Buffers in,work,out;for(int ch=0;ch<16;++ch)for(unsigned j=0;j<n;++j)in.write[ch][j]=0.01*std::sin(2*pi*1000*j/rate+ch*0.1);
        const char* configs[]={"unity","IIR-AP","FIR8193","FIR8193-AP","filter-transition","emergency-mute","topology-disable","IIR-FIR-transition","FIR-length-transition","program-transition"};
        for(int module=0;module<4;++module){const char* modules[]={"Crossover","OutputMatrix","Headroom","AudioGraph"};if(variant>=4&&(module==1||module==2))continue;
            if(rowIndex++<firstRow)continue;++executedRows;
            bool transition=variant>=4;std::vector<double> timing(transition?200:10000);unsigned deadlines=0;
            auto refill=[&](){if(module==0){for(int ch=0;ch<13;++ch)std::copy_n(in.read[ch],n,work.write[ch]);for(int ch=13;ch<16;++ch)std::fill_n(work.write[ch],n,0.0);}};
            auto run=[&](){if(module==0){entropy::native::AudioBuffer<double> v(work.write.data(),16,n);entropy::native::MidiBuffer midi;cross->processBlock(v,midi);return ENTROPY_OK;}if(module==1){auto b=block(in,out,n);return m.process(b);}if(module==2){auto b=block(in,out,n);return h.process(b);}return graph.process(in.read.data(),out.write.data(),n);};
            auto change=[&](unsigned i,bool opposite){bool high=bool(i%2)^opposite;const char* key=variant==4?"lpHz":variant==5?"emergencyMute":variant==6?(program?"cinemaRole":"source"):variant==7?"processMode":variant==8?"firLength":"cinemaSource";double value=variant==4?(high?12000:10000):variant==8?(high?2:0):high?1:0;
                if(variant==9){if(module==0)set(*cross,key,value);else graph.setOfflineParameter(key,value);}
                else for(int ch=0;ch<16;++ch){std::string id=std::string(key)+std::to_string(ch+1);if(module==0)set(*cross,id.c_str(),value);else graph.setOfflineParameter(id.c_str(),value);}
                if(module==0)cross->serviceUpdates();else graph.applyOfflineParameters();};
            for(int i=0;i<2000;++i){refill();require(run()==ENTROPY_OK,"benchmark warmup");}
            for(unsigned i=0;i<timing.size();++i){if(transition){change(i,false);for(unsigned warm=0;warm<(19200+n-1)/n;++warm){refill();require(run()==ENTROPY_OK,"transition adoption warmup");}
                // Restore the opposite state, settle, then measure the next adoption callback.
                change(i,true);}
                refill();auto start=now();auto r=run();timing[i]=double(now()-start)*1e6/freq.QuadPart;require(r==ENTROPY_OK,"benchmark process");deadlines+=timing[i]>n/rate*1e6;
            }
            std::sort(timing.begin(),timing.end());size_t k=static_cast<size_t>(std::ceil(timing.size()*0.99))-1;double median=(timing[timing.size()/2-1]+timing[timing.size()/2])/2,budget=n/rate*1e6;
            csv<<modules[module]<<','<<program<<','<<configs[variant]<<','<<n<<','<<timing.size()<<','<<median<<','<<timing[k]<<','<<timing.back()<<','<<budget<<','<<timing.back()/budget<<','<<(deadlines?"FAIL":"PASS")<<'\n';csv.flush();std::printf("BENCH %s program=%d %s block=%u median=%.3f p99=%.3f max=%.3f us deadline_misses=%u\n",modules[module],program,configs[variant],n,median,timing[k],timing.back(),deadlines);std::fflush(stdout);
            if(deadlines)++failures;
        }
    }if(!executedRows)return 2;return failures?1:0;
}
}
int main(int argc,char** argv){
    try {if(argc<3){std::fprintf(stderr,"usage: EntropyStage02 --identity|--numerical|--transitions|--realtime|--benchmark output-directory [verified-V07-path] [--quick]\n");return 2;}
        ippInit();auto* v=ippGetLibVersion();std::printf("IPP %s %s %s\n",v->Name,v->Version,v->BuildDate);Ipp64u features=0;ippGetCpuFeatures(&features,nullptr);std::printf("IPP cpu_features=0x%llx enabled=0x%llx MSVC=%d\n",features,ippGetEnabledCpuFeatures(),_MSC_FULL_VER);
        const char* gold=nullptr;bool quick=false;std::string mode=argv[1];
        unsigned shard=0,shards=1,firstGroup=0,firstBenchmarkRow=0;for(int i=3;i<argc;++i){std::string a=argv[i];if(a=="--quick")quick=true;else if(a.starts_with("--shard=")){if(std::sscanf(a.c_str(),"--shard=%u/%u",&shard,&shards)!=2||!shards||shard>=shards)throw std::runtime_error("invalid shard");}else if(a.starts_with("--from-group=")){if(std::sscanf(a.c_str(),"--from-group=%u",&firstGroup)!=1||firstGroup>112)throw std::runtime_error("invalid first group");}else if(a.starts_with("--from-benchmark-row=")){if(std::sscanf(a.c_str(),"--from-benchmark-row=%u",&firstBenchmarkRow)!=1||firstBenchmarkRow>224)throw std::runtime_error("invalid benchmark row");}else if(a.starts_with("--")||gold)throw std::runtime_error("unknown or duplicate argument");else gold=argv[i];}
        if(mode=="--identity")return stage02::identity(argv[2]);if(mode=="--numerical")return stage02::numerical(argv[2],gold,quick,shard,shards,firstGroup);if(mode=="--transitions")return stage02::transitions(argv[2],gold);if(mode=="--controls")return stage02::controls(argv[2]);if(mode=="--realtime")return stage02::realtime(argv[2]);if(mode=="--benchmark")return stage02::benchmark(argv[2],firstBenchmarkRow);
        if(mode=="--cpu-baseline"){
            Ipp64u mask=ippCPUID_MMX|ippCPUID_SSE|ippCPUID_SSE2|ippCPUID_SSE3|ippCPUID_SSSE3|ippCPUID_SSE41|ippCPUID_SSE42|ippCPUID_AES|ippCPUID_CLMUL|ippCPUID_SHA;
            mask&=features;auto status=ippSetCpuFeatures(mask);std::printf("SSE42 restricted dispatch requested=0x%llx status=%d enabled=0x%llx\n",mask,status,ippGetEnabledCpuFeatures());if(status!=ippStsNoErr)return 2;
            int result=stage02::numerical(argv[2],nullptr,true);auto after=ippGetEnabledCpuFeatures();std::printf("SSE42 final enabled=0x%llx\n",after);if(after&(ippCPUID_AVX|ippCPUID_AVX2)){std::fprintf(stderr,"BLOCKED restricted dispatch reset by backend initialization\n");return 2;}return result;
        }
        return 2;
    } catch(const std::exception& e){std::fprintf(stderr,"BLOCKED/ERROR %s\n",e.what());return 2;}
}
