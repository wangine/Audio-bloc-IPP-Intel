#include "Stage02/Fixtures.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numbers>
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    using namespace stage02;int failures=0;
    LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    auto now=[](){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;};
    auto csvOpen=[](const std::string& path){std::ofstream s(path);s.exceptions(std::ios::failbit|std::ios::badbit);s<<std::setprecision(12);return s;};
    try{
        auto raw=csvOpen(std::string(argv[1])+"/fade-samples.csv");raw<<"module,program,configuration,frames,round,callback,wall_us,thread_cycles\n";
        auto csv=csvOpen(std::string(argv[1])+"/fade-benchmark.csv");csv<<"module,program,configuration,frames,count,median_us,p99_us,max_us,median_thread_cycles,p99_thread_cycles,max_thread_cycles,budget_us,deadline_misses,status\n";
        for(int program=0;program<2;++program)for(unsigned frames:{128u,256u,512u,1024u})for(int variant=0;variant<5;++variant)for(int module=0;module<2;++module){
            Configuration c{program,1,2,true,true,true};
            auto cross=std::make_unique<Crossover>();cross->prepareToPlay(rate,1024);set(*cross,"cinemaSource",program);cross->serviceUpdates();configure(c,[&](const char* k,double v){return set(*cross,k,v);});cross->prepareToPlay(rate,1024);
            auto graph=std::make_unique<entropy::engine::AudioGraph>();if(!graph->prepare(1024))return 2;graph->setOfflineParameter("cinemaSource",program);graph->applyOfflineParameters();configure(c,[&](const char* k,double v){return graph->setOfflineParameter(k,v);});graph->applyOfflineParameters(true);
            if(variant==4){for(int scene=0;scene<2;++scene){auto other=c;other.cinema=scene;set(*cross,"cinemaSource",scene);cross->serviceUpdates();configure(other,[&](const char* k,double v){return set(*cross,k,v);});cross->serviceUpdates();graph->setOfflineParameter("cinemaSource",scene);graph->applyOfflineParameters();configure(other,[&](const char* k,double v){return graph->setOfflineParameter(k,v);});graph->applyOfflineParameters();}set(*cross,"cinemaSource",program);cross->serviceUpdates();cross->prepareToPlay(rate,1024);graph->setOfflineParameter("cinemaSource",program);graph->applyOfflineParameters(true);}
            Buffers in,work,out;uint64_t position=0;
            auto refill=[&](){for(int ch=0;ch<13;++ch)for(unsigned j=0;j<frames;++j)in.write[ch][j]=0.01*std::sin(2*std::numbers::pi*1000*(position+j)/rate+ch*0.1);if(!module){for(int ch=0;ch<13;++ch)std::copy_n(in.read[ch],frames,work.write[ch]);for(int ch=13;ch<16;++ch)std::fill_n(work.write[ch],frames,0.0);}position+=frames;};
            auto run=[&](){if(module)return graph->process(in.read.data(),out.write.data(),frames);entropy::native::AudioBuffer<double> b(work.write.data(),16,frames);entropy::native::MidiBuffer midi;cross->processBlock(b,midi);return ENTROPY_OK;};
            auto change=[&](bool high){const char* key=variant==0?"lpHz":variant==1?"processMode":variant==2?"firLength":variant==3?"allPass":"cinemaSource";double value=variant==0?(high?12000:10000):variant==2?(high?2:0):high?1:0;
                auto put=[&](const char* id){bool ok=module?graph->setOfflineParameter(id,value):set(*cross,id,value);if(!ok)throw std::runtime_error("fade parameter rejected");};
                if(variant==4)put(key);
                else for(int ch=0;ch<16;++ch){auto id=std::string(key)+std::to_string(ch+1);put(id.c_str());}
                if(module){if(!graph->applyOfflineParameters())throw std::runtime_error("fade update rejected");}else cross->serviceUpdates();};
            for(int i=0;i<2000;++i){refill();if(run()!=ENTROPY_OK)return 2;}
            const unsigned callbacks=(19200+frames-1)/frames;
            std::vector<double> wall;std::vector<ULONG64> cycles;wall.reserve(20*callbacks);cycles.reserve(20*callbacks);unsigned misses=0;
            const char* configs[]={"filter-fade","IIR-FIR-fade","FIR-length-fade","FIR-AllPass-fade","program-fade"};const char* name=module?"AudioGraph":"Crossover";
            std::array<double,16> peaks{};
            for(unsigned round=0;round<20;++round){bool high=bool(round%2);if(variant==0)high=!high;if(variant==4)high=round%2?bool(program):!bool(program);change(high);for(unsigned callback=0;callback<callbacks;++callback){refill();ULONG64 a=0,b=0;if(!QueryThreadCycleTime(GetCurrentThread(),&a))return 2;auto start=now();auto result=run();double us=double(now()-start)*1e6/frequency.QuadPart;if(!QueryThreadCycleTime(GetCurrentThread(),&b)||result!=ENTROPY_OK)return 2;wall.push_back(us);cycles.push_back(b-a);misses+=us>frames/rate*1e6;
                    const auto& data=module?out:work;for(int ch=0;ch<16;++ch)for(unsigned j=0;j<frames;++j){double x=data.read[ch][j];if(!std::isfinite(x))return 2;peaks[ch]=std::max(peaks[ch],std::abs(x));}
                }}
            for(double peak:peaks)if(peak<=1e-10)throw std::runtime_error("fade active output silent");
            for(size_t i=0;i<wall.size();++i)raw<<name<<','<<program<<','<<configs[variant]<<','<<frames<<','<<i/callbacks<<','<<i%callbacks<<','<<wall[i]<<','<<cycles[i]<<'\n';raw.flush();
            std::sort(wall.begin(),wall.end());std::sort(cycles.begin(),cycles.end());size_t middle=wall.size()/2,p99=static_cast<size_t>(std::ceil(wall.size()*0.99))-1;double median=wall.size()%2?wall[middle]:(wall[middle-1]+wall[middle])/2;double cycleMedian=cycles.size()%2?double(cycles[middle]):cycles[middle-1]/2.0+cycles[middle]/2.0;
            csv<<name<<','<<program<<','<<configs[variant]<<','<<frames<<','<<wall.size()<<','<<median<<','<<wall[p99]<<','<<wall.back()<<','<<cycleMedian<<','<<cycles[p99]<<','<<cycles.back()<<','<<frames/rate*1e6<<','<<misses<<','<<(misses?"FAIL":"PASS")<<'\n';csv.flush();
            std::printf("FADE %s program=%d %s frames=%u median=%.3f p99=%.3f max=%.3f us misses=%u\n",name,program,configs[variant],frames,median,wall[p99],wall.back(),misses);std::fflush(stdout);if(misses)++failures;
        }return failures?1:0;
    }catch(const std::exception& e){std::fprintf(stderr,"BLOCKED %s\n",e.what());return 2;}
}
