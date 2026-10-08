#include "Engine/Egress/NativeEgress.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
#include <windows.h>
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <EgressClockEstimator.h>

using namespace entropy::egress_host;
namespace audit=entropy::host::allocation_audit;
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct FakeSink final:Sink {
    enum Failure {NoFailure,Open,Start,Wait,Submit,Lost,ShortSubmit,Oversized};
    Failure failure=NoFailure;EntropyDacSinkConfig config{};bool opened=false,running=false;
    uint64_t position=0;unsigned submitted=0,stops=0,closes=0;std::vector<uint8_t> last;
    EntropyResult open(const EntropyDacSinkConfig& c) override {
        if(failure==Open)return ENTROPY_ERROR_PROCESSING;
        config=c;opened=true;last.assign(size_t(c.requested_buffer_frames)*c.channels*(c.output_format?4:3),0);return ENTROPY_OK;
    }
    EntropyResult start() noexcept override {if(failure==Start)return ENTROPY_ERROR_PROCESSING;running=true;return ENTROPY_OK;}
    EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override {
        if(failure==Wait||failure==Lost){running=false;return ENTROPY_ERROR_PROCESSING;}
        r.frames=config.requested_buffer_frames+(failure==Oversized?1:0);r.bytes=r.frames*config.channels*(config.output_format?4:3);
        r.device_position_frames=position;r.device_qpc_position=10000000+position*10000000/192000;position+=config.requested_buffer_frames;return ENTROPY_OK;
    }
    EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override {
        if(failure==Submit)return ENTROPY_ERROR_PROCESSING;
        if(b.byte_count!=last.size())return ENTROPY_ERROR_INVALID_ARGUMENT;
        std::memcpy(last.data(),b.data,b.byte_count);++submitted;
        b.accepted_frames=b.frames-(failure==ShortSubmit?1:0);return ENTROPY_OK;
    }
    EntropyDacSinkStatus status() const noexcept override {
        EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;
        s.buffer_frames=config.requested_buffer_frames;return s;
    }
    void stop() noexcept override {running=false;++stops;}
    void close() noexcept override {opened=false;++closes;}
};
EntropyDacSinkConfig sinkConfig(unsigned frames,unsigned format=0){
    EntropyDacSinkConfig c{};c.struct_size=sizeof(c);c.backend=ENTROPY_DAC_SINK_BACKEND_WASAPI_EXCLUSIVE;
    c.sample_rate=192000;c.channels=2;c.output_format=format;c.requested_buffer_frames=frames;c.revision=1;
    std::strcpy(c.endpoint_id_utf8,"offline-fake-endpoint");return c;
}
void lifecycle(std::ostream& log) {
    for(unsigned fmt=0;fmt<2;++fmt) for(int fail=0;fail<=7;++fail){
        FirstDacGraph graph;ProbeConfig cfg;cfg.format=fmt;check(graph.prepare(cfg),"fake graph prepare");
        check(graph.arm(),"ARM valid");check(!graph.arm(),"duplicate ARM");
        for(unsigned i=0;i<16;++i)check(graph.produce(),"prebuffer");
        FakeSink sink;sink.failure=static_cast<FakeSink::Failure>(fail);SinkOwner owner(graph.pipeline(),sink);
        if(fail==FakeSink::Open){check(!owner.open(sinkConfig(256,fmt)),"open failure");}
        else {
            check(owner.open(sinkConfig(256,fmt)),"fake open");
            if(fail==FakeSink::Start)check(!owner.start(),"start failure");
            else {
                check(owner.start(),"safe startup");
                if(fail){check(!owner.cycle(),"driver fault accepted");check(!sink.running&&!sink.opened,"fault shutdown");}
                else {
                    for(unsigned i=0;i<100;++i){check(graph.produce(),"normal graph");check(owner.cycle(),"normal render");}
                    check(std::any_of(sink.last.begin(),sink.last.end(),[](auto v){return v!=0;}),"active PCM mute");
                    graph.disarm();check(!owner.cycle(),"DISARM");
                    check(std::all_of(sink.last.begin(),sink.last.end(),[](auto v){return v==0;}),"DISARM silence");
                }
            }
        }
        owner.stop();check(!sink.running&&!sink.opened,"closed owner");
        if(fail)check(graph.pipeline().state()==State::Fault && (graph.pipeline().faults()&Device),"device fault latched");
        check(!graph.arm(),"no implicit rearm");
        check(graph.prepare(cfg)&&graph.arm(),"explicit prepare/recovery");
        graph.disarm();log<<"lifecycle format="<<fmt<<" injected="<<fail<<" PASS\n";
    }
    FirstDacGraph graph;ProbeConfig cfg;cfg.firTaps=8193;cfg.outputChannels=16;
    check(!graph.prepare(cfg),"FIR8193x16 forbidden");cfg={};cfg.amplitude=.010001;
    check(!graph.prepare(cfg),"invalid input bound");cfg={};check(graph.prepare(cfg)&&graph.arm(),"prepare starvation");
    auto& p=graph.pipeline();for(unsigned i=0;i<16;++i)check(graph.produce(),"starvation prefill");
    p.feedUntilTarget(256);check(p.render(256,0,10000000),"startup render");
    bool starved=false;for(unsigned i=1;i<200;++i){p.feedUntilTarget(256);if(!p.render(256,i*256,10000000+uint64_t(i)*256*10000000/192000)){starved=true;break;}}
    check(starved && p.state()==State::Fault && p.xruns()>0 && (p.faults()&Starvation),"starvation not visible");
    check(std::all_of(p.bytes(256).begin(),p.bytes(256).end(),[](auto v){return v==0;}),"starvation silence");
    check(!graph.produce()&&!graph.arm(),"starvation rearm forbidden");
    cfg={};check(graph.prepare(cfg)&&graph.arm(),"recover starvation");
    std::array<std::array<double,256>,11> bad{};std::array<const double*,11> src{};
    for(unsigned c=0;c<11;++c)src[c]=bad[c].data();bad[4][17]=std::numeric_limits<double>::quiet_NaN();
    check(!graph.produce(src.data()) && (p.faults()&Input),"nonfinite fail closed");
    check(!p.render(256,0,0)&&std::all_of(p.bytes(256).begin(),p.bytes(256).end(),[](auto v){return v==0;}),"input fault silence");
    DacPipeline raw;PipelineConfig rc;rc.maximumFrames=256;check(raw.prepare(rc)&&raw.arm(true,true),"raw overflow prepare");
    std::array<double,256> zeros{};const double* lanes[]={zeros.data(),zeros.data()};
    for(unsigned i=0;i<rc.fifoFrames/256;++i)check(raw.publish(lanes,256),"FIFO full fill");
    check(!raw.publish(lanes,256)&&(raw.faults()&Overflow)&&raw.xruns()==1,"FIFO overflow fail closed");
    check(!raw.arm(true,true),"overflow auto rearm");
    raw.disarm();check(raw.prepare(rc)&&raw.arm(true,true),"raw unsafe PCM prepare");
    zeros.fill(2.0);for(unsigned i=0;i<16;++i){check(raw.publish(lanes,256)&&raw.transfer(256),"raw rail input");}
    check(!raw.render(256,0,0)&&(raw.faults()&Pcm),"unsafe PCM rejected before converter");
    check(raw.pcmTelemetry().reserved[0]==0,"no rail clamp");
    std::array<double,4> signedZero{0.0,-0.0,1,-1},copy{};const double* in[]={signedZero.data()};double* out[]={copy.data()};
    DacPipeline::guardianBypass(in,out,1,4);check(std::memcmp(signedZero.data(),copy.data(),sizeof(copy))==0,"Guardian signed-zero identity");
    log<<"starvation/overflow/nonfinite/rail-clamp/Guardian/FIR rejection PASS\n";
    for(unsigned renderFrames:{128u,256u,512u,1024u}){
        FirstDacGraph mixed;ProbeConfig pc;check(mixed.prepare(pc)&&mixed.arm(),"mixed buffer prepare");
        for(unsigned i=0;i<16;++i)check(mixed.produce(),"mixed preroll");
        FakeSink sink;SinkOwner owner(mixed.pipeline(),sink);
        check(owner.open(sinkConfig(renderFrames))&&owner.start(),"negotiated render size");
        for(unsigned i=0;i<100;++i){
            const unsigned blocks=renderFrames>=256?renderFrames/256:(i%2==0?1:0);
            for(unsigned b=0;b<blocks;++b)check(mixed.produce(),"mixed producer");
            check(owner.cycle(),"mixed render cycle");
        }
        check(std::any_of(sink.last.begin(),sink.last.end(),[](auto v){return v!=0;}),"mixed buffer muted");
        owner.stop();log<<"Graph256 / negotiated render"<<renderFrames<<" PASS\n";
    }
}

void numerical(const std::filesystem::path& root) {
    std::ofstream csv(root/"numerical.csv");csv.precision(17);
    csv<<"program,format,frames,signal,channel,max_abs,rms_error,amplitude,reference_amplitude,phase_difference_rad,latency_difference_samples,first_active,bit_mismatches,pcm_byte_mismatches,nonfinite,status\n";
    for(unsigned program=0;program<2;++program)for(unsigned fmt=0;fmt<2;++fmt)for(unsigned n:{128u,256u,512u,1024u})for(unsigned signal=0;signal<3;++signal){
        FirstDacGraph host;ProbeConfig cfg;cfg.frames=n;cfg.program=program;cfg.format=fmt;
        check(host.prepare(cfg)&&host.arm(),"numerical prepare");
        entropy::engine::AudioGraph reference;
        check(reference.prepare(n)&&FirstDacGraph::configureProbeGraph(reference,program),"oracle graph");
        entropy::engine::PlanarBuffer src,out,refClock;src.prepare(13,n);out.prepare(16,n);refClock.prepare(2,n);
        entropy::egress::EgressClockEngine clock;check(clock.configure(host.pipeline().clockConfig()),"oracle clock");
        entropy::pcm24::Pcm24ModuleEngine pcm;EntropyPcm24Config pc{};
        pc.struct_size=sizeof(pc);pc.channels=2;pc.output_format=fmt;pc.maximum_frames=n;pc.revision=1;
        check(pcm.configure(pc),"oracle PCM");
        std::vector<uint8_t> refBytes(size_t(n)*2*(fmt?4:3));uint32_t rng=12345;
        double max[2]{},err2[2]{},peak[2]{},refPeak[2]{},ss[2]{},cs[2]{},rs[2]{},rcs[2]{};
        uint64_t bits[2]{},bytes=0,nan[2]{},count=0;int64_t first[2]{-1,-1},refFirst[2]{-1,-1};
        for(unsigned block=0;block<400;++block){
            src.clear(n);
            for(unsigned f=0;f<n;++f){uint64_t at=uint64_t(block)*n+f;double v;
                if(signal==0)v=.01*std::sin(6.2831853071795864769*1000*double(at)/192000);
                else if(signal==1)v=at%32768==0?.01:0;
                else{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;v=.01*(double(rng)/4294967295.0*2-1);}
                src.channel(program?3:0)[f]=v;src.channel(program?4:1)[f]=v;
            }
            check(host.produce(src.readPointers()),"numeric producer");
            check(reference.process(src.readPointers(),out.writePointers(),n)==ENTROPY_OK,"oracle graph process");
            for(unsigned c=0;c<2;++c)for(unsigned f=0;f<n;++f)out.channel(c)[f]*=.1;
            const double* oracle[]={out.readChannel(0),out.readChannel(1)};
            check(host.pipeline().transfer(n),"numeric transfer");
            EntropyEgressClockPushBlockF64 push{};push.struct_size=sizeof(push);push.channels=2;push.frames=n;push.inputs=oracle;
            check(clock.push(push)==ENTROPY_OK,"oracle push");
            auto pos=uint64_t(block)*n,time=10000000+pos*10000000/192000;
            check(host.pipeline().render(n,pos,time),"numeric render");
            EntropyEgressClockRenderBlockF64 render{};render.struct_size=sizeof(render);render.channels=2;render.frames=n;render.outputs=refClock.writePointers();render.sink_position_frames=pos;render.host_time_100ns=time;
            check(clock.render(render)==ENTROPY_OK,"oracle render");
            EntropyPcm24BlockF64 pb{};pb.struct_size=sizeof(pb);pb.channels=2;pb.frames=n;pb.inputs=refClock.readPointers();pb.output_interleaved=refBytes.data();pb.output_capacity_bytes=static_cast<unsigned>(refBytes.size());
            check(pcm.process(pb)==ENTROPY_OK,"oracle PCM process");
            auto actual=host.pipeline().bytes(n);for(size_t i=0;i<actual.size();++i)bytes+=actual[i]!=refBytes[i];
            for(unsigned c=0;c<2;++c)for(unsigned f=0;f<n;++f){double a=host.pipeline().rendered().readChannel(c)[f],b=refClock.readChannel(c)[f];
                auto at=uint64_t(block)*n+f;double e=a-b;max[c]=(std::max)(max[c],std::abs(e));err2[c]+=e*e;
                peak[c]=(std::max)(peak[c],std::abs(a));refPeak[c]=(std::max)(refPeak[c],std::abs(b));
                bits[c]+=std::bit_cast<uint64_t>(a)!=std::bit_cast<uint64_t>(b);nan[c]+=!std::isfinite(a)||!std::isfinite(b);
                if(first[c]<0&&std::abs(a)>1e-10)first[c]=at;if(refFirst[c]<0&&std::abs(b)>1e-10)refFirst[c]=at;
                if(block>=32){double phase=6.2831853071795864769*1000*double(at)/192000;
                    ss[c]+=a*std::sin(phase);cs[c]+=a*std::cos(phase);rs[c]+=b*std::sin(phase);rcs[c]+=b*std::cos(phase);}
            }count+=n;
        }
        for(unsigned c=0;c<2;++c){bool pass=max[c]==0&&bits[c]==0&&bytes==0&&nan[c]==0&&peak[c]>1e-10&&first[c]==refFirst[c];
            csv<<program<<','<<fmt<<','<<n<<','<<signal<<','<<c<<','<<max[c]<<','<<std::sqrt(err2[c]/count)<<','
               <<2*std::hypot(ss[c],cs[c])/(368*n)<<','<<2*std::hypot(rs[c],rcs[c])/(368*n)<<','
               <<std::atan2(cs[c],ss[c])-std::atan2(rcs[c],rs[c])<<','<<first[c]-refFirst[c]<<','<<first[c]<<','<<bits[c]<<','<<bytes<<','<<nan[c]<<','<<(pass?"PASS":"FAIL")<<'\n';
            check(pass,"numerical difference or muted output");
        }
        host.disarm();
    }
}

void concurrent(std::ostream& log) {
    FirstDacGraph graph;ProbeConfig cfg;check(graph.prepare(cfg)&&graph.arm(),"concurrent prepare");
    auto& p=graph.pipeline();for(unsigned i=0;i<16;++i)check(graph.produce(),"concurrent prefill");
    std::atomic<unsigned> request{0},done{0};std::atomic<bool> good{true};
    std::thread producer([&]{for(unsigned i=1;i<=2000;++i){while(request.load(std::memory_order_acquire)<i)std::this_thread::yield();
        {audit::CycleScope phase;if(!graph.produce())good=false;}done.store(i,std::memory_order_release);}});
    for(unsigned i=1;i<=2000;++i){request.store(i,std::memory_order_release);while(done.load(std::memory_order_acquire)<i)std::this_thread::yield();
        {audit::CycleScope phase;p.feedUntilTarget(256);if(!p.render(256,uint64_t(i)*256,10000000+uint64_t(i)*256*10000000/192000))good=false;}}
    producer.join();check(good,"two-owner graph/sink failed");graph.disarm();
    log<<"two-owner DSP Graph / DAC Sink 2000 cycles PASS; handshake waits are outside audit callback\n";
}

void overlapping(std::ostream& log) {
    FirstDacGraph graph;ProbeConfig cfg;check(graph.prepare(cfg)&&graph.arm(),"overlap prepare");
    auto& p=graph.pipeline();for(unsigned i=0;i<16;++i)check(graph.produce(),"overlap prefill");
    std::atomic<bool> finish{false},good{true};
    std::atomic<unsigned> produced{0};
    std::thread producer([&]{while(!finish.load(std::memory_order_acquire)){
        // Backpressure belongs to the offline driver, outside the audited callback.
        if(p.writable()<12000){std::this_thread::yield();continue;}
        {audit::CycleScope scope;if(!graph.produce()){good=false;finish=true;break;}}
        produced.fetch_add(1,std::memory_order_release);
    }});
    for(unsigned i=0;i<2000 && good;++i){
        while(p.readable()<256 && good)std::this_thread::yield();
        {audit::CycleScope scope;p.feedUntilTarget(256);
            if(!p.render(256,uint64_t(i)*256,10000000+uint64_t(i)*256*10000000/192000)){good=false;break;}}
    }
    finish=true;producer.join();graph.disarm();check(good&&produced>0,"overlapping owners failed");
    p.fail(Device);graph.disarm();check(p.state()==State::Fault,"disarm must retain Fault");
    log<<"overlapping producer/consumer 2000 renders PASS; produced="<<produced<<"; no per-block acknowledgment\n";
}

void clockDomains(const std::filesystem::path& root) {
    std::ofstream csv(root/"clock-domains.csv");csv.precision(17);
    csv<<"frames,domain,drift_ppm,observations,valid_estimates,estimated_rate,last_confidence,status\n";
    for(unsigned frames:{128u,256u,512u,1024u}){
        entropy::egress::ClockEstimator a,b;
        check(a.prepare(192000)&&b.prepare(192000),"independent estimator prepare");
        unsigned valid[2]{};entropy::egress::ClockEstimate last[2]{};
        for(unsigned i=0;i<20000;++i)for(unsigned domain=0;domain<2;++domain){
            const double rate=192000*(1+(domain?100.0:-100.0)*1e-6);
            entropy::egress::ClockObservation o;
            o.sinkFrame=uint64_t(i)*frames;
            o.host100ns=10000000+uint64_t(double(o.sinkFrame)*1e7/rate);
            o.nominalPublished=uint64_t(i)*frames;o.renderFrames=frames;o.fillBefore=2048;o.continuous=true;
            last[domain]=(domain?b:a).observe(o);valid[domain]+=last[domain].valid;
        }
        for(unsigned d=0;d<2;++d)csv<<frames<<','<<d<<','<<(d?100:-100)<<",20000,"<<valid[d]<<','<<last[d].sinkFramesPerSecond<<','<<last[d].confidence<<','<<(valid[d]?"PASS":"FAIL_NO_VALID_ESTIMATE")<<'\n';
    }
}

struct Sample{double us;uint64_t cycles;};
template<class F> void measure(std::ostream& csv,std::ostream& raw,const char* module,unsigned program,unsigned format,unsigned n,F run,bool& deadlines) {
    for(unsigned i=0;i<2000;++i)check(run(),"benchmark warmup");
    constexpr unsigned loops=10000;std::vector<Sample> data(loops);LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
    for(unsigned i=0;i<loops;++i){LARGE_INTEGER a,b;ULONG64 ca=0,cb=0;bool ok;
        {audit::CycleScope phase;QueryThreadCycleTime(GetCurrentThread(),&ca);QueryPerformanceCounter(&a);ok=run();QueryPerformanceCounter(&b);QueryThreadCycleTime(GetCurrentThread(),&cb);}
        check(ok,"benchmark processing failed");data[i]={double(b.QuadPart-a.QuadPart)*1e6/frequency.QuadPart,cb-ca};}
    auto counts=audit::snapshot();unsigned misses=0;for(unsigned i=0;i<loops;++i){auto s=data[i];misses+=s.us>double(n)*1e6/192000;raw<<module<<','<<program<<','<<format<<','<<n<<','<<i<<','<<s.us<<','<<s.cycles<<'\n';}
    std::vector<double> wall;std::vector<uint64_t> cycles;wall.reserve(loops);cycles.reserve(loops);
    for(auto s:data){wall.push_back(s.us);cycles.push_back(s.cycles);}std::sort(wall.begin(),wall.end());std::sort(cycles.begin(),cycles.end());
    csv<<module<<','<<program<<','<<format<<','<<n<<','<<(wall[4999]+wall[5000])/2<<','<<wall[9899]<<','<<wall.back()<<','
       <<(double(cycles[4999])+double(cycles[5000]))/2<<','<<cycles[9899]<<','<<cycles.back()<<','<<misses<<','<<(misses?"FAIL":"PASS")<<'\n';
    std::printf("BENCH %s program=%u format=%u frames=%u median=%.3f p99=%.3f max=%.3f us misses=%u\n",module,program,format,n,(wall[4999]+wall[5000])/2,wall[9899],wall.back(),misses);
    std::fflush(stdout);if(misses)deadlines=false;
    check(counts.cppNew==0&&counts.cppDelete==0&&counts.heapAlloc==0&&counts.heapFree==0&&counts.wait==0&&counts.lock==0&&counts.io==0,"realtime safety audit");
}
bool benchmark(const std::filesystem::path& root){
    std::ofstream csv(root/"benchmark.csv"),raw(root/"benchmark-samples.csv");csv.precision(17);raw.precision(17);
    csv<<"module,program,format,frames,median_us,p99_us,max_us,median_thread_cycles,p99_thread_cycles,max_thread_cycles,deadline_misses,status\n";
    raw<<"module,program,format,frames,iteration,wall_us,thread_cycles\n";bool deadlines=true;
    for(unsigned program=0;program<2;++program)for(unsigned fmt=0;fmt<2;++fmt)for(unsigned n:{128u,256u,512u,1024u}){
        FirstDacGraph graph;ProbeConfig cfg;cfg.frames=n;cfg.program=program;cfg.format=fmt;check(graph.prepare(cfg)&&graph.arm(),"bench graph prepare");
        auto& p=graph.pipeline();uint64_t position=0;
        measure(csv,raw,"Complete",program,fmt,n,[&]{bool ok=graph.produce()&&p.transfer(n)&&p.render(n,position,10000000+position*10000000/192000);position+=n;return ok;},deadlines);
        graph.disarm();
        entropy::engine::AudioGraph g;check(g.prepare(n)&&FirstDacGraph::configureProbeGraph(g,program)&&g.applyEgressHeadroomPolicy(FirstDacGraph::probeLedger(),20),"bench graph");
        entropy::engine::PlanarBuffer src,out;src.prepare(13,n);out.prepare(16,n);src.clear(n);
        for(unsigned f=0;f<n;++f)src.channel(0)[f]=src.channel(1)[f]=src.channel(3)[f]=src.channel(4)[f]=.001*std::sin(6.2831853071795864769*1000*f/192000);
        measure(csv,raw,"AudioGraph",program,fmt,n,[&]{return g.process(src.readPointers(),out.writePointers(),n)==ENTROPY_OK;},deadlines);
        SpscAudioFifo fifo;check(fifo.prepare(2,16384),"bench FIFO prepare");
        measure(csv,raw,"FIFO",program,fmt,n,[&]{return fifo.push(src.readPointers(),n)&&fifo.pop(out.writePointers(),n);},deadlines);
        entropy::egress::EgressClockEngine clock;check(clock.configure(p.clockConfig()),"bench clock prepare");position=0;
        EntropyEgressClockPushBlockF64 push{};push.struct_size=sizeof(push);push.channels=2;push.frames=n;push.inputs=src.readPointers();
        EntropyEgressClockRenderBlockF64 render{};render.struct_size=sizeof(render);render.channels=2;render.frames=n;render.outputs=out.writePointers();
        measure(csv,raw,"ClockSync",program,fmt,n,[&]{render.sink_position_frames=position;render.host_time_100ns=10000000+position*10000000/192000;position+=n;return clock.push(push)==ENTROPY_OK&&clock.render(render)==ENTROPY_OK&&render.state!=ENTROPY_EGRESS_CLOCK_STARVED;},deadlines);
        entropy::pcm24::Pcm24ModuleEngine pcm;EntropyPcm24Config pc{};pc.struct_size=sizeof(pc);pc.channels=2;pc.maximum_frames=n;pc.output_format=fmt;
        check(pcm.configure(pc),"bench PCM prepare");std::vector<uint8_t> bytes(size_t(n)*2*(fmt?4:3));
        EntropyPcm24BlockF64 pb{};pb.struct_size=sizeof(pb);pb.channels=2;pb.frames=n;pb.inputs=src.readPointers();pb.output_interleaved=bytes.data();pb.output_capacity_bytes=static_cast<unsigned>(bytes.size());
        measure(csv,raw,"PCM24",program,fmt,n,[&]{return pcm.process(pb)==ENTROPY_OK;},deadlines);
    }
    return deadlines;
}
}
int main(int argc,char** argv){
    try {
        const std::filesystem::path root=argc>1?argv[1]:"stage04-results";std::filesystem::create_directories(root);
        check(audit::selfTest(),"audit self test");audit::reset();
        std::ofstream log(root/"contracts.txt");lifecycle(log);numerical(root);clockDomains(root);concurrent(log);overlapping(log);
        auto counts=audit::snapshot();check(counts.cppNew==0&&counts.cppDelete==0&&counts.heapAlloc==0&&counts.heapFree==0&&counts.wait==0&&counts.lock==0&&counts.io==0,"two-thread audit");
        audit::report();std::puts("PASS Stage04 offline contracts, numerical bytes, two-owner audit; no devices enumerated/opened");
        if(argc>2 && std::strcmp(argv[2],"--benchmark")==0){audit::reset();bool pass=benchmark(root);audit::report();return pass?0:1;}
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL Stage04: %s\n",e.what());return 2;}
}
