// Bounded offline PCM24 evidence. No IPC, publisher attachment or WASAPI sink.
#include "Engine/Control/ProfileStore.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include <entropy/stage_bus_v1.h>
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>
#include <windows.h>
using namespace entropy;
using J=ipc::Value;
constexpr double rate=192000,pi=3.14159265358979323846,lsb=1.0/8388608.0;
constexpr unsigned frames=1024;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static int32_t decode(const uint8_t* bytes,unsigned format){
    if(format)check(bytes[0]==0,"PCM24-in-32 lost left alignment");
    bytes+=format?1:0;
    uint32_t value=uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16);
    if(value&0x800000)value|=0xff000000u;
    return std::bit_cast<int32_t>(value);
}
static J converter(){
    // One 1-second two-tone fragment; the ultrasonic component is already in FP64.
    constexpr unsigned count=192000,channels=2;
    constexpr std::array<double,7> frequencies{1000,2000,3000,50000,51000,60000,90000};
    std::array<std::array<double,2>,7> inputBins{},errorBins{};
    std::vector<int32_t> reference(count*channels);J::Array formats;
    for(unsigned format=0;format<2;++format){
        pcm24::Pcm24ModuleEngine engine;EntropyPcm24Config config{};
        config.struct_size=sizeof(config);config.channels=channels;config.maximum_frames=frames;config.output_format=format;
        config.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;config.seed_mode=ENTROPY_PCM24_SEED_DETERMINISTIC_TEST;
        config.reserved_u32=ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT;config.seed=0x5354414745303134ULL;
        check(engine.configure(config),"offline converter configuration failed");
        std::array<std::array<double,frames>,channels> input{};const double* planes[]{input[0].data(),input[1].data()};
        std::array<uint8_t,frames*channels*4> packed{};double maxError=0,sum=0,energy=0,inputPeak=0;uint64_t samples=0;
        for(unsigned at=0;at<count;at+=frames){const unsigned size=std::min(frames,count-at);
            for(unsigned f=0;f<size;++f)for(unsigned c=0;c<channels;++c){const double t=(at+f)/rate;
                input[c][f]=.001*std::sin(2*pi*1000*t+.2*c)+.00001*std::sin(2*pi*50000*t+.3*c);}
            EntropyPcm24BlockF64 block{};block.struct_size=sizeof(block);block.channels=channels;block.frames=size;
            block.inputs=planes;block.output_interleaved=packed.data();block.output_capacity_bytes=unsigned(packed.size());
            check(engine.process(block)==ENTROPY_OK&&block.produced_bytes==size*channels*(format?4:3),"converter processing failed");
            for(unsigned f=0;f<size;++f)for(unsigned c=0;c<channels;++c){const auto q=decode(packed.data()+(f*channels+c)*(format?4:3),format);
                const double x=input[c][f],error=q*lsb-x;check(std::isfinite(error)&&std::abs(error)<=1.5*lsb,"sub-rail TPDF error exceeds 1.5 LSB");
                if(!format)reference[(at+f)*channels+c]=q;else check(q==reference[(at+f)*channels+c],"packed and left-aligned formats differ");
                maxError=std::max(maxError,std::abs(error));sum+=error;energy+=error*error;inputPeak=std::max(inputPeak,std::abs(x));++samples;
                if(!format&&c==0)for(unsigned k=0;k<frequencies.size();++k){const double angle=2*pi*frequencies[k]*(at+f)/rate;
                    inputBins[k][0]+=x*std::cos(angle);inputBins[k][1]+=x*std::sin(angle);
                    errorBins[k][0]+=error*std::cos(angle);errorBins[k][1]+=error*std::sin(angle);}
            }
        }
        const auto t=engine.telemetry();check(t.fault_flags==0&&t.overrange_sample_count==0&&t.nonfinite_sample_count==0&&t.reserved[0]==0&&t.reserved[2]==0,"sub-rail converter counters are not clean");
        formats.emplace_back(J::Object{{"format",format},{"samples",J::u64(samples)},{"inputPeak",inputPeak},{"maxErrorLsb",maxError/lsb},{"rmsErrorLsb",std::sqrt(energy/samples)/lsb},{"meanErrorLsb",sum/samples/lsb},{"integratedErrorRmsDbfs",20*std::log10(std::sqrt(energy/samples))},{"pcmFaults",t.fault_flags},{"pcmOverrangeSamples",J::u64(t.overrange_sample_count)},{"pcmClippedBlocks",J::u64(t.reserved[2])},{"pcmRailClampSamples",J::u64(t.reserved[0])},{"pcmNonfiniteSamples",J::u64(t.nonfinite_sample_count)}});
    }
    J::Array bins;for(unsigned k=0;k<frequencies.size();++k){const double before=2*std::hypot(inputBins[k][0],inputBins[k][1])/count,err=2*std::hypot(errorBins[k][0],errorBins[k][1])/count;
        bins.emplace_back(J::Object{{"frequencyHz",frequencies[k]},{"beforeConverterPeakAmplitude",before},{"introducedErrorPeakAmplitude",err},{"introducedErrorBinDbfs",20*std::log10(err)}});}
    return J::Object{{"status","PASS"},{"seconds",1},{"dither","TPDF_FLAT"},{"seed","DETERMINISTIC_TEST: offline only"},{"formats",formats},{"packedAndLeftAlignedBitExact",true},{"selectedCoherentBins",bins},{"fullScaleSineSnrTheoryDb",20*std::log10((1/std::sqrt(2.0))/(.5*lsb))}};
}
struct Recorded{StageBusBlockHeader header{};std::array<std::array<double,frames>,11> samples{};};
struct Limits{double peak=0,energy=0;uint64_t overrange=0,samples=0;void add(double x){check(std::isfinite(x),"replay contains NaN/Inf");peak=std::max(peak,std::abs(x));energy+=x*x;overrange+=std::abs(x)>1;++samples;}J json()const{return J::Object{{"peak",peak},{"rms",samples?std::sqrt(energy/samples):0},{"overrangeSamples",J::u64(overrange)},{"samples",J::u64(samples)}};}};
static J replay(const char* profilePath,const char* rawPath){
    control::ProfileStore profiles;auto original=profiles.load(std::filesystem::u8path(profilePath));
    constexpr unsigned blocks=1125;std::vector<Recorded> recorded(blocks);
    std::ifstream raw(std::filesystem::u8path(rawPath),std::ios::binary);raw.read(reinterpret_cast<char*>(recorded.data()),std::streamsize(recorded.size()*sizeof(Recorded)));
    check(bool(raw)&&raw.peek()==EOF,"existing six-second capture size mismatch");
    engine::AudioGraph graph;check(graph.prepare(frames),"offline Graph prepare failed");control::ControlPlane controls(graph);
    controls.crossover().importBytes(original.crossover);auto pre=original.parameters;pre.mode=ENTROPY_HEADROOM_MODE_BIT_PERFECT;controls.replace(pre);
    const auto domains=control::LiveSinkWorkers::decode(original.document.at("stage05_devices").at("configuration"));
    controls.setRenderLaneMask(control::LiveSinkWorkers::renderLaneMask(domains));
    engine::PlanarBuffer input,matrix,post;input.prepare(13,frames);matrix.prepare(16,frames);post.prepare(16,frames);input.clear(frames);
    for(unsigned warm=0;warm<200;++warm){controls.atBlockBoundary();check(graph.process(input.readPointers(),matrix.writePointers(),frames)==ENTROPY_OK,"Graph warmup failed");controls.afterBlock();if(warm>32&&controls.settled())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(controls.settled(),"real profile not settled");
    headroom::HeadroomEngine headroom;check(headroom.prepare(rate,frames),"Headroom prepare failed");
    check(headroom.setState(controls.appliedState().headroom)==ENTROPY_OK&&headroom.setTiming(original.parameters.timing)==ENTROPY_OK&&headroom.setManualGainDb(original.parameters.manualGainDb)==ENTROPY_OK&&headroom.setMode(original.parameters.mode)==ENTROPY_OK,"real Headroom state rejected");
    EntropyAudioBlockF64 audio{};audio.struct_size=sizeof(audio);audio.input_channels=audio.output_channels=16;audio.frames=frames;audio.inputs=matrix.readPointers();audio.outputs=post.writePointers();
    matrix.clear(frames);for(unsigned warm=0;warm<256;++warm){check(headroom.process(audio)==ENTROPY_OK,"Headroom warmup failed");if(warm>1&&!headroom.modeState().transition_active)break;}
    check(!headroom.modeState().transition_active,"Headroom startup transition unresolved");
    std::array<egress_host::DacPipeline,2> pipeline;std::array<Limits,2> preLimits{},postLimits{},clockLimits{};std::array<uint64_t,2> rendered{};
    for(unsigned d=0;d<2;++d)if(domains[d].enabled){egress_host::PipelineConfig config;config.channels=domains[d].channels;config.format=domains[d].format;config.fifoFrames=32768;config.targetFrames=4096;
        check(pipeline[d].prepare(config)&&pipeline[d].arm(true,true),"offline clock pipeline prepare failed");}
    const bool music=controls.crossover().snapshot().at("program").asUnsigned(1)==0;double prePostError=0;uint64_t sequence=0;
    for(const auto& record:recorded){check(record.header.frames==frames,"capture block size mismatch");check(!sequence||record.header.block_sequence==sequence+1,"capture sequence gap");sequence=record.header.block_sequence;
        for(unsigned ch=0;ch<11;++ch)std::copy(record.samples[ch].begin(),record.samples[ch].end(),input.channel(ch));
        engine::GraphControlAccess::crossover(graph)->setProgramInputEpochs(record.header.music_session_revision,record.header.source_generation[0],record.header.source_generation[2],0xffffu);
        controls.atBlockBoundary();check(graph.process(input.readPointers(),matrix.writePointers(),frames)==ENTROPY_OK,"captured Graph processing failed");controls.afterBlock();
        if(music){EntropyHeadroomAnalysisBlockF64 analysis{sizeof(analysis),16,frames,0,matrix.readPointers()};check(headroom.analyzeFinal(analysis)==ENTROPY_OK,"original Headroom analysis failed");}
        check(headroom.process(audio)==ENTROPY_OK,"original Headroom process failed");
        for(unsigned d=0;d<2;++d)if(domains[d].enabled){std::array<const double*,16> selected{};
            for(unsigned c=0;c<domains[d].channels;++c){const auto lane=domains[d].lanes[c];selected[c]=post.readChannel(lane);for(unsigned f=0;f<frames;++f){const double a=matrix.readChannel(lane)[f],b=post.readChannel(lane)[f];preLimits[d].add(a);postLimits[d].add(b);prePostError=std::max(prePostError,std::abs(a-b));}}
            auto& pipe=pipeline[d];check(pipe.publish(selected.data(),frames),"offline publish failed");pipe.feedUntilTarget(frames);
            // Same prebuffer requirement as live; no sink is instantiated.
            if(!rendered[d]&&pipe.clockTelemetry().nominal_frames<4096+frames+100)continue;
            check(pipe.render(frames,rendered[d],10000000+rendered[d]*10000000/192000),"offline ClockSync render failed");rendered[d]+=frames;
            for(unsigned c=0;c<domains[d].channels;++c)for(unsigned f=0;f<frames;++f)clockLimits[d].add(pipe.rendered().readChannel(c)[f]);
        }
    }
    J::Array rows;bool overrange=false;
    for(unsigned d=0;d<2;++d)if(domains[d].enabled){check(clockLimits[d].energy>0,"silent offline DAC cannot pass");const auto pcm=pipeline[d].pcmTelemetry();overrange|=postLimits[d].overrange!=0||clockLimits[d].overrange!=0||pcm.overrange_sample_count!=0;
        rows.emplace_back(J::Object{{"id",d?"DAC B":"DAC A"},{"preHeadroom",preLimits[d].json()},{"postHeadroom",postLimits[d].json()},{"postClockSync",clockLimits[d].json()},{"pcmOverrangeSamples",J::u64(pcm.overrange_sample_count)},{"pcmClippedBlocks",J::u64(pcm.reserved[2])},{"pcmRailClampSamples",J::u64(pcm.reserved[0])},{"pcmNonfiniteSamples",J::u64(pcm.nonfinite_sample_count)},{"pcmFaults",pcm.fault_flags},{"clockRenderedFrames",J::u64(rendered[d])}});}
    if(original.parameters.mode==ENTROPY_HEADROOM_MODE_BIT_PERFECT)check(prePostError==0,"BITPERFECT Headroom gained/attenuated audio");
    return J::Object{{"status",overrange?"FAIL POST-HEADROOM/PCM OVERRANGE":"PASS MEASURED SUBSET"},{"captureSeconds",6},{"graphFrames",blocks*frames},{"layerWindowsIdentical",false},{"coverage","Headroom covers full capture; ClockSync covers rendered subset after preroll, with buffered tail unflushed. RMS differences are not a gain measurement."},{"headroomMode",original.parameters.mode},{"manualControlDb",original.parameters.manualGainDb},{"prePostHeadroomMaxDifference",prePostError},{"domains",rows},{"clockModel","independent nominal 192k clocks; no hardware scheduling or live phase reconstruction"},{"profileChanged",false}};
}
static void manualCopy(const char* source,const std::filesystem::path& destination,double gainDb){
    check(std::isfinite(gainDb)&&gainDb>=-60&&gainDb<=0,"explicit manual comparison gain outside range");
    check(!std::filesystem::exists(destination),"preserve existing comparison profile");
    control::ProfileStore profiles;auto original=profiles.load(std::filesystem::u8path(source));auto document=original.document;
    struct Persisted{uint32_t magic,version;EntropyHeadroomState state;uint32_t mode,reserved;double gain;};
    Persisted h{};h.magic=0x35524845;h.version=5;h.state=original.parameters.headroom;h.mode=ENTROPY_HEADROOM_MODE_MANUAL_SAFE;h.gain=gainDb;
    std::vector<unsigned char> bytes(sizeof(h)+sizeof(original.parameters.timing));std::memcpy(bytes.data(),&h,sizeof(h));std::memcpy(bytes.data()+sizeof(h),&original.parameters.timing,sizeof(original.parameters.timing));
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string encoded;
    for(size_t i=0;i<bytes.size();i+=3){unsigned n=unsigned(bytes[i])<<16;if(i+1<bytes.size())n|=unsigned(bytes[i+1])<<8;if(i+2<bytes.size())n|=bytes[i+2];encoded+=alphabet[(n>>18)&63];encoded+=alphabet[(n>>12)&63];encoded+=i+1<bytes.size()?alphabet[(n>>6)&63]:'=';encoded+=i+2<bytes.size()?alphabet[n&63]:'=';}
    uint64_t sum=14695981039346656037ULL;for(auto byte:bytes)sum=(sum^byte)*1099511628211ULL;
    std::ostringstream checksum;checksum<<std::hex<<std::setw(16)<<std::setfill('0')<<sum;
    bool found=false;for(auto& module:document["modules"].asArray())if(module.at("instance_id").asString()=="headroom.main"){module["serialized_state"]=encoded;module["state_checksum_fnv1a64"]=checksum.str();found=true;}
    check(found,"profile Headroom payload missing");{std::ofstream out(destination);out<<ipc::dump(document);check(bool(out),"manual comparison profile write failed");}
    const auto copy=profiles.load(destination);check(copy.crossover==original.crossover&&copy.parameters.mode==ENTROPY_HEADROOM_MODE_MANUAL_SAFE&&copy.parameters.manualGainDb==gainDb,"comparison profile verification failed");
    auto restored=copy.document;for(size_t i=0;i<restored["modules"].asArray().size();++i)if(restored["modules"].asArray()[i].at("instance_id").asString()=="headroom.main")restored["modules"].asArray()[i]=original.document.at("modules").asArray()[i];
    check(ipc::dump(restored)==ipc::dump(original.document),"comparison modified unrelated module/device/metadata");
}
int main(int argc,char** argv)try{
    const bool replayOnly=argc==5&&std::string(argv[1])=="--replay-only",copy=argc==6&&std::string(argv[1])=="--manual-copy";
    check(argc==1||argc==4||replayOnly||copy,"Usage: EntropyStage014Pcm24 [actual.entropy existing-fragment.raw output.json] | --replay-only actual raw json | --manual-copy actual raw json explicit-dB");
    SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
    J result=J::Object{{"hardwareOpened",false},{"productionSourcesChanged",false}};
    if(!replayOnly&&!copy)result["converter"]=converter();
    if(argc!=1){const unsigned offset=replayOnly||copy?1:0;std::string profile=argv[1+offset];const auto output=std::filesystem::u8path(argv[3+offset]);
        if(copy){const auto path=output.parent_path()/"MANUAL-SAFE-comparison.entropy";const double db=std::stod(argv[5]);manualCopy(profile.c_str(),path,db);profile=path.string();result["comparison"]=J::Object{{"mode","MANUAL_SAFE"},{"explicitGainDb",db},{"originalProfileChanged",false},{"onlyHeadroomPolicyChanged",true},{"liveLoaded",false}};}
        result["overloadReplay"]=replay(profile.c_str(),argv[2+offset]);std::ofstream out(output);out<<ipc::dump(result);check(bool(out),"evidence write failed");}
    std::cout<<ipc::dump(result)<<'\n';
    return argc!=1&&result.at("overloadReplay").at("status").asString()!="PASS MEASURED SUBSET"?2:0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
