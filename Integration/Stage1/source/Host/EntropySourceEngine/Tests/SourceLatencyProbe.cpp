// Bounded synthetic diagnostic; instrumentation is linked only into test executables.
#include "SourceDomainStream.h"
#include "SourceBlockExecutor.h"
#include "SourceCycleTimer.h"
#include "RealtimeBufferPolicy.h"
#include "EngineCadencePolicy.h"
#include "ProAudioThreadScope.h"
#include "StageBusWin32.h"
#include <windows.h>
#include <entropy/src_abi.h>
#ifdef ENTROPY_SRC_WORKER_TRACE
#include "SrcWorkerTrace.h"
#endif
#ifdef ENTROPY_SRC_BIT_ORACLE
#include "SrcAudioOracle.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
using namespace entropy::host;
using namespace entropy::stage;
static uint64_t tick(){LARGE_INTEGER v;QueryPerformanceCounter(&v);return v.QuadPart;}
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<" "<<error<<"\n";return 2;}}while(0)
static void distribution(std::ostream& o,std::vector<double>& v){if(v.empty()){o<<"null";return;}std::sort(v.begin(),v.end());o<<"{\"p50\":"<<v[(v.size()-1)*50/100]<<",\"p95\":"<<v[(v.size()-1)*95/100]<<",\"p99\":"<<v[(v.size()-1)*99/100]<<",\"max\":"<<v.back()<<"}";}
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"rate seconds output-prefix\n";return 2;}
 const unsigned rate=std::stoul(argv[1]),seconds=std::stoul(argv[2]); const unsigned engine=GetEnvironmentVariableA("ENTROPY_LATENCY_ENGINE",nullptr,0)?std::stoul(std::getenv("ENTROPY_LATENCY_ENGINE")):0;std::string error,prefix=argv[3];
 REQUIRE(rate==44100||rate==48000||rate==88200||rate==96000||rate==176400||rate==192000);REQUIRE(seconds>=2);
 for(const auto* suffix:{".json",".f64","-worker.csv","-demand.csv","-trace.json"}) REQUIRE(!std::filesystem::exists(prefix+suffix));
 SetEnvironmentVariableA("ENTROPY_LATENCY_RATE",argv[1]);
 auto epLib=ModuleLibrary::load(TEST_LATENCY_ENDPOINT,error);REQUIRE(epLib);
 auto srcLib=ModuleLibrary::load(std::filesystem::path(TEST_MODULE_ROOT)/"EntropySampleRateConverter.dll",error);REQUIRE(srcLib);
 ModuleInstance ep(epLib),src(srcLib);REQUIRE(ep.create(error)&&ep.prepare(192000,1024,2,2,error));REQUIRE(src.create(error)&&src.prepare(192000,1024,2,2,error));
 auto native=GetModuleHandleA(TEST_LATENCY_ENDPOINT);auto beginFn=reinterpret_cast<uint64_t(*)(EntropyModuleHandle)>(GetProcAddress(native,"entropy_latency_begin"));REQUIRE(beginFn);

 ModuleInstance startupSrc(srcLib);REQUIRE(startupSrc.create(error)&&startupSrc.prepare(192000,1024,2,2,error));
 EntropySrcApiV1 startupApi{};startupApi.struct_size=sizeof(startupApi);REQUIRE(startupSrc.queryExtension(ENTROPY_SRC_EXTENSION_ID,ENTROPY_SRC_EXTENSION_VERSION,&startupApi,sizeof(startupApi),error));
 EntropySrcConfig startupConfig{};startupConfig.struct_size=sizeof(startupConfig);startupConfig.engine=engine;startupConfig.source_rate=rate;startupConfig.destination_rate=192000;startupConfig.channels=2;startupConfig.maximum_input_frames=256;REQUIRE(startupApi.configure(startupSrc.handle(),&startupConfig)==ENTROPY_OK);
 std::array<double,256> startupInput{};std::array<const double*,2> startupInputs{startupInput.data(),startupInput.data()};std::vector<double> startupLeft(startupConfig.required_output_capacity),startupRight(startupConfig.required_output_capacity);std::array<double*,2> startupOutputs{startupLeft.data(),startupRight.data()};uint64_t firstOutputInputFrames=0;
 for(unsigned packet=1;packet<=1024;++packet){EntropySrcBlockF64 b{};b.struct_size=sizeof(b);b.channels=2;b.input_frames=256;b.output_capacity=startupConfig.required_output_capacity;b.inputs=startupInputs.data();b.outputs=startupOutputs.data();REQUIRE(startupApi.process(startupSrc.handle(),&b)==ENTROPY_OK&&b.consumed_frames==256);if(b.produced_frames){firstOutputInputFrames=packet*256;break;}}
 REQUIRE(firstOutputInputFrames);
 SourceGraphRuntime graph;REQUIRE(graph.prepare(TEST_MODULE_ROOT,error));SourceBlockExecutor executor;REQUIRE(executor.prepare(graph,error));
 SourceDomainConfig config;config.channels=2;config.srcEngine=engine;config.fifoCapacityFrames=RealtimeBufferPolicy::sourceCapacityFrames;config.captureFifoCapacityFrames=RealtimeBufferPolicy::sourceCaptureCapacityFrames;config.prerollFrames=RealtimeBufferPolicy::sourcePrerollFrames;config.outputTargetFrames=RealtimeBufferPolicy::sourceTargetFrames;
 if(const auto* probePreroll=std::getenv("ENTROPY_LATENCY_PREROLL"))config.prerollFrames=std::stoul(probePreroll);
 if(const auto* probeTarget=std::getenv("ENTROPY_LATENCY_TARGET"))config.outputTargetFrames=std::stoul(probeTarget);
 uint64_t bitComparedBlocks=0,bitMismatchBlocks=0;
#ifdef ENTROPY_SRC_BIT_ORACLE
 SrcAudioOracle oracle(srcLib); REQUIRE(oracle.prepare(rate,TEST_MODULE_ROOT,error,engine));
#endif
 SourceDomainStream stream;REQUIRE(stream.prepare(ep,src,config));
 LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);const auto hz=uint64_t(frequency.QuadPart);
 uint8_t uuid[16]{};auto token=tick();std::memcpy(uuid,&token,8);uuid[8]=109;auto session=makeSession(1,uuid,hz);StageBusWin32 producer,consumer;REQUIRE(producer.create(session,error)&&consumer.open(session,error));
 const uint64_t blocks=(uint64_t(seconds)*192000+1023)/1024+1;
#ifdef ENTROPY_SRC_WORKER_TRACE
 src_trace::prepare(size_t((std::min)(seconds,60u)+5)*4096);
 struct Demand { uint64_t qpc; uint32_t capture,converted; bool ready; };
 std::vector<Demand> demands; demands.reserve(blocks);
#endif
 std::vector<double> process,roundtrip,spacing,lateness;process.reserve(blocks);roundtrip.reserve(blocks);spacing.reserve(blocks);lateness.reserve(blocks);
 // Audio storage is bounded to two seconds; disk writes happen after stream stop.
 std::vector<double> dump;dump.reserve(375*1024*10);
 std::array<std::array<double,1024>,2> in{};std::array<double*,2> ip{in[0].data(),in[1].data()};
 std::array<std::array<double,1024>,10> out{},received{};std::array<double*,10> op{},rp{};std::array<const double*,10> cp{};for(unsigned c=0;c<10;++c){op[c]=out[c].data();rp[c]=received[c].data();cp[c]=op[c];}
 ProAudioThreadScope realtime;SourceCycleTimer timer;REQUIRE(timer.valid());REQUIRE(stream.start());auto timeout=tick()+5*hz;while(!stream.ready()){REQUIRE(tick()<timeout);Sleep(1);}
 const auto playBegin=tick(),sourceBegin=beginFn(ep.handle());
 const auto epoch=std::chrono::steady_clock::now();auto scheduled=epoch;uint64_t lastDemand=0,previousGeneration=1,stagebusFaults=0;
 uint64_t missing=0,badSequence=0,faults=0,generationChanges=0,discontinuities=0,completed=0;uint32_t minFill=UINT32_MAX,maxFill=0,minCapture=UINT32_MAX,maxCapture=0;double sumFill=0,peak=0;uint64_t peakIndex=0,peakQpc=0;SourceDomainTelemetry telemetry;
 for(uint64_t seq=1;seq<=blocks;++seq){
  while(std::chrono::steady_clock::now()<scheduled)timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::microseconds(200));
  const auto blockBegin=std::chrono::steady_clock::now();const auto demandTick=tick();if(lastDemand)spacing.push_back(double(demandTick-lastDemand)*1e6/hz);lastDemand=demandTick;lateness.push_back(std::chrono::duration<double,std::micro>(blockBegin-scheduled).count());
  telemetry=stream.telemetry();
#ifdef ENTROPY_SRC_WORKER_TRACE
  demands.push_back({demandTick,telemetry.capture_fifo.fill_frames,telemetry.fifo.fill_frames,stream.ready()});
#endif
  discontinuities|=telemetry.discontinuity_flags;minFill=(std::min)(minFill,telemetry.fifo.fill_frames);maxFill=(std::max)(maxFill,telemetry.fifo.fill_frames);sumFill+=telemetry.fifo.fill_frames;minCapture=(std::min)(minCapture,telemetry.capture_fifo.fill_frames);maxCapture=(std::max)(maxCapture,telemetry.capture_fifo.fill_frames);
  StageBusFrameRequest request{};std::memcpy(request.session_id,uuid,16);request.stream_epoch=1;request.block_sequence=seq;request.first_frame=(seq-1)*1024;request.control_revision=1;request.frames=1024;
  consumer.heartbeat(tick());producer.heartbeat(tick());auto sent=tick();if(!(consumer.trySendSourceRequest(request)==StageBusIoResult::Ok)){++stagebusFaults;break;}StageBusFrameRequest incoming{};if(!(producer.tryReceiveSourceRequest(incoming)==StageBusIoResult::Ok)){++stagebusFaults;break;}
  SourceStreamDescriptor descriptor;if(!stream.pop1024(ip.data(),descriptor)){++missing;break;}if(descriptor.generation!=previousGeneration){++generationChanges;previousGeneration=descriptor.generation;}
  AudioBlockReport music{},empty{};music.passed=true;music.convertedAudioPointerFrames=1024;music.streamGeneration=descriptor.generation;music.state=2;music.sourceRate=rate;music.channels=2;music.convertedAudioPointers[0]=ip[0];music.convertedAudioPointers[1]=ip[1];
  auto before=tick();auto result=executor.process(SourceFrameEnvelope(incoming),0,music,empty,empty,op.data());process.push_back(double(tick()-before)*1e6/hz);REQUIRE(result.passed);result.header.qpc_timestamp=tick();if(!(producer.tryPublish(result.header,cp.data())==StageBusIoResult::Ok)){++stagebusFaults;break;}StageBusBlockHeader h{};if(!(consumer.tryConsume(h,rp.data())==StageBusIoResult::Ok)){++stagebusFaults;break;}auto arrived=tick();roundtrip.push_back(double(arrived-sent)*1e6/hz);
  if(h.block_sequence!=seq||h.first_frame!=request.first_frame)++badSequence;
#ifdef ENTROPY_SRC_BIT_ORACLE
  if(!oracle.next()||!oracle.sourceIdentical(ip.data())||!oracle.outputIdentical(incoming,music,empty,rp.data())) ++bitMismatchBlocks;
  ++bitComparedBlocks;
#endif
  for(unsigned i=0;i<1024;++i)if(std::abs(received[0][i])>peak){peak=std::abs(received[0][i]);peakIndex=(seq-1)*1024+i;peakQpc=arrived;}
  if(seq<=375)for(unsigned c=0;c<10;++c)dump.insert(dump.end(),received[c].begin(),received[c].end());
  scheduled=EngineCadencePolicy::boundedDeadline(epoch+std::chrono::nanoseconds(EngineCadencePolicy::deadlineNanoseconds(0,seq)),std::chrono::steady_clock::now(),std::chrono::nanoseconds(1024ull*1000000000/192000),blockBegin);
  ++completed;if(bitMismatchBlocks||telemetry.fifo.underruns||telemetry.fifo.overflows||telemetry.capture_fifo.overflows||telemetry.endpoint_lost_frames||telemetry.faults){++faults;break;}
 }
 telemetry=stream.telemetry();stream.stop();discontinuities|=telemetry.discontinuity_flags;auto elapsed=double(tick()-sourceBegin)/hz;
 #ifdef ENTROPY_SRC_WORKER_TRACE
 src_trace::write(prefix,hz);
 std::ofstream demandFile(prefix+"-demand.csv"); demandFile<<"qpc,capture,converted,ready\n";
 for(const auto& d:demands) demandFile<<d.qpc<<','<<d.capture<<','<<d.converted<<','<<d.ready<<'\n';
#endif
 const bool markerObserved=peak>0.01;bool pass=markerObserved&&!bitMismatchBlocks&&!stagebusFaults&&completed==blocks&&!missing&&!badSequence&&!faults&&!generationChanges&&!discontinuities&&!telemetry.fifo.underruns&&!telemetry.fifo.overflows&&!telemetry.capture_fifo.overflows&&!telemetry.endpoint_lost_frames&&!telemetry.faults;

#ifdef ENTROPY_SRC_WORKER_TRACE
 pass=pass&&src_trace::workerProAudioActive&&src_trace::captureProAudioActive.load();
#endif
 const double markerSource=double(rate/4)/rate;const double batchDelay=double(peakQpc-sourceBegin)/hz-markerSource;
 std::ofstream audio(prefix+".f64",std::ios::binary);audio.write(reinterpret_cast<const char*>(dump.data()),dump.size()*sizeof(double));REQUIRE(audio.good());
 std::ofstream json(prefix+".json");json<<std::setprecision(12);json<<"{\"src_first_output_input_frames\":"<<firstOutputInputFrames<<",\"src_first_output_accumulation_ms\":"<<double(firstOutputInputFrames)*1000/rate<<",\"capture_packet_frames\":256,\"pass\":"<<(pass?"true":"false")<<",\"rate\":"<<rate<<",\"engine\":"<<engine<<",\"preroll\":"<<config.prerollFrames<<",\"target\":"<<config.outputTargetFrames<<",\"block_frames\":1024,\"src_input_frames\":256,\"seconds\":"<<elapsed<<",\"blocks\":"<<completed<<",\"src_reported_latency_frames\":"<<telemetry.src_latency_frames<<",\"source_fifo_current\":"<<telemetry.fifo.fill_frames<<",\"source_fifo_low_water\":"<<telemetry.fifo.low_water_frames<<",\"source_fifo_high_water\":"<<telemetry.fifo.high_water_frames<<",\"sampled_prepop_min\":"<<minFill<<",\"sampled_prepop_mean\":"<<sumFill/lateness.size()<<",\"sampled_prepop_max\":"<<maxFill<<",\"source_fifo_underruns\":"<<telemetry.fifo.underruns<<",\"source_fifo_overflows\":"<<telemetry.fifo.overflows<<",\"capture_fifo_current\":"<<telemetry.capture_fifo.fill_frames<<",\"capture_fifo_low_water\":"<<telemetry.capture_fifo.low_water_frames<<",\"capture_fifo_high_water\":"<<telemetry.capture_fifo.high_water_frames<<",\"capture_fifo_overflows\":"<<telemetry.capture_fifo.overflows<<",\"capture_empty_polls\":"<<telemetry.capture_fifo.underruns<<",\"endpoint_lost_frames\":"<<telemetry.endpoint_lost_frames<<",\"source_faults\":"<<telemetry.faults<<",\"source_generation_changes\":"<<generationChanges<<",\"startup_resets\":"<<telemetry.resets<<",\"unexpected_discontinuities\":"<<discontinuities<<",\"missing_blocks\":"<<missing<<",\"sequence_errors\":"<<badSequence<<",\"stagebus_faults\":"<<stagebusFaults<<",\"processing_us\":";distribution(json,process);json<<",\"inprocess_stagebus_roundtrip_us\":";distribution(json,roundtrip);json<<",\"demand_spacing_us\":";distribution(json,spacing);json<<",\"demand_lateness_us\":";distribution(json,lateness);json<<",\"mmcss_active\":"<<(realtime.active()?"true":"false");
 json<<",\"impulse_output_index\":"<<(markerObserved?std::to_string(peakIndex):"null")<<",\"impulse_peak\":"<<peak<<",\"signal_offset_samples\":"<<(markerObserved?std::to_string(double(peakIndex)-markerSource*192000):"null")<<",\"startup_to_first_demand_ms\":"<<double(playBegin-sourceBegin)*1000/hz<<",\"impulse_batch_delivery_delay_samples\":"<<(markerObserved?std::to_string(batchDelay*192000):"null")<<",\"impulse_batch_delivery_delay_ms\":"<<(markerObserved?std::to_string(batchDelay*1000):"null")<<",\"bit_compared_blocks\":"<<bitComparedBlocks<<",\"bit_mismatch_blocks\":"<<bitMismatchBlocks<<",\"demand_elapsed_seconds\":"<<double(lastDemand-playBegin)/hz<<",\"endpoint_frames_read\":"<<telemetry.endpoint_frames_read<<",\"src_input_frames_total\":"<<telemetry.src_input_frames<<",\"src_output_frames\":"<<telemetry.src_output_frames<<",\"src_input_fps\":"<<double(telemetry.src_input_frames)/elapsed<<",\"src_output_fps\":"<<double(telemetry.src_output_frames)/elapsed<<",\"limitations\":\"Synthetic lossless paced endpoint; batch arrival timing, no physical endpoint or downstream device; in-process V1 mapping timing; SRC telemetry zero does not prove zero startup delay.\"}";REQUIRE(json.good());
 std::cout<<(pass?"PASS":"FAIL")<<" rate="<<rate<<" blocks="<<completed<<" missing="<<missing<<" underruns="<<telemetry.fifo.underruns<<" capture_overflows="<<telemetry.capture_fifo.overflows<<" batch_delay_ms="<<(markerObserved?std::to_string(batchDelay*1000):"null")<<"\n";return pass?0:1;
}
