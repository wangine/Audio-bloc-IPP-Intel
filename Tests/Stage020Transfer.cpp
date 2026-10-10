// A bounded OFFLINE replay: no publisher, WASAPI, Qt or live IPC entry point.
#include "Engine/Control/ProfileStore.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/NativeEgress.h"
#include <entropy/stage_bus_v1.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <thread>
using namespace entropy;using J=ipc::Value;using Access=engine::GraphControlAccess;
constexpr unsigned N=1024,blocks=1125;constexpr double Fs=192000;
struct Recorded{StageBusBlockHeader header;std::array<std::array<double,N>,11> samples;};
struct HeadBlock{std::array<std::array<double,N>,16> samples;};
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
J loadJson(const std::filesystem::path& p){std::ifstream f(p);return ipc::parse(std::string(std::istreambuf_iterator<char>(f),{}));}
void saveJson(const std::filesystem::path& p,const J& j){std::ofstream f(p);f<<ipc::dump(j);require(bool(f),"JSON output");}
J section(const standalone::visual::Section& c){return J::Array{c.b0,c.b1,c.b2,c.a1,c.a2};}
J modelJson(const standalone::visual::Configuration& m,const J& snapshot){J::Array channels;const auto& outputs=snapshot.at("outputs").asArray();
 for(unsigned i=0;i<16;++i){const auto& c=m.channels[i];J::Array hp,lp,a,head,tail;
  for(unsigned k=0;k<c.stages;++k){hp.emplace_back(section(c.hp[k]));lp.emplace_back(section(c.lp[k]));}for(int k=0;k<=c.delay.order;++k)a.emplace_back(c.delay.a[k]);
  if(c.fir){for(auto v:c.kernel.head)head.emplace_back(v);for(int part=0;part<c.kernel.partitions;++part){J::Array row;for(auto v:c.kernel.tail[part])row.emplace_back(J::Array{v.re,v.im});tail.emplace_back(std::move(row));}}
  channels.emplace_back(J::Object{{"channel",i},{"role",outputs[i].at("role")},{"enabled",c.enabled},{"gain",c.gain},{"bypass",c.bypass},{"hpOn",c.hpOn},{"lpOn",c.lpOn},{"apOn",c.apOn},{"fir",c.fir},{"hp",hp},{"lp",lp},{"ap",section(c.ap)},{"firHead",head},{"firTailSpectra",tail},{"delaySamples",c.delay.delay},{"delayMode",c.delay.mode},{"delayOrder",c.delay.order},{"integerTap",c.delay.integerTap},{"delayCoefficients",a}});
 }return J::Object{{"generation",J::u64(m.generation)},{"sampleRate",m.sampleRate},{"channels",channels}};
}
void writeTap(std::ofstream& f,const double* const* p,unsigned channels){for(unsigned c=0;c<channels;++c)f.write(reinterpret_cast<const char*>(p[c]),N*sizeof(double));require(bool(f),"tap write");}
J asrcReplay(const std::vector<HeadBlock>& h,const std::vector<Recorded>& source,const J& active,const std::filesystem::path& root){J::Array summary;const auto& domains=active.at("telemetry").at("data").at("domains").asArray();
 for(unsigned slot=0;slot<domains.size();++slot){const auto& d=domains[slot];if(!d.at("configured").asBool())continue;const unsigned channels=d.at("channels").asUnsigned();const double rate=d.at("estimatedSinkRateHz").asNumber();
  auto config=active.at("profileDevices").asArray()[slot];const auto& lanes=config.at("lanes").asArray();egress_host::DacPipeline p;egress_host::PipelineConfig pc;pc.channels=channels;pc.maximumFrames=N;pc.fifoFrames=d.at("fifoCapacity").asUnsigned();pc.targetFrames=4096;pc.format=d.at("format").asUnsigned();
  require(p.prepare(pc)&&p.prepareSilentCalibration(),"ASRC inactive prepare");uint64_t position=0;bool calibrated=false;
  auto time=[&](uint64_t n){return uint64_t(100000000+double(n)*10000000/rate);};
  for(unsigned step=0;step<1800&&!calibrated;++step){position+=N;calibrated=p.observeSilentClock(position,time(position),N);}
  require(calibrated&&p.arm(true,true),"original clock calibration");
  // Rebase the OFFLINE clock origin to zero; source identity is retained in
  // identity.csv. Subtracting the 559-million-frame absolute origin otherwise
  // loses fractional bits before the coefficient oracle sees the read phase.
  unsigned next=0;auto publish=[&]{std::array<const double*,16> pointers{};for(unsigned c=0;c<channels;++c)pointers[c]=h[next].samples[lanes[c].asUnsigned(15)].data();require(p.publish(pointers.data(),N,{source[next].header.stream_epoch,uint64_t(next)*N,source[next].header.qpc_timestamp,N}),"ASRC source publish");++next;};
  while(next<12)publish();require(p.primeGraphPosition(256,N),"declared ASRC first position");
  std::ofstream pcm(root/("post-asrc-"+std::to_string(slot)+".f64"),std::ios::binary);std::ofstream positions(root/("asrc-positions-"+std::to_string(slot)+".csv"));positions<<std::setprecision(17)<<"outputFrame,firstGraphFrame,endGraphFrame,epoch,stepBegin,stepEnd,ppm,resync\n";
  double maximum=0,square=0;uint64_t count=0,rendered=0;
  auto value=[&](unsigned lane,int64_t sample){if(sample<0||sample>=int64_t(blocks)*N)return 0.0;return h[unsigned(sample)/N].samples[lane][unsigned(sample)%N];};
  for(unsigned cycle=0;cycle<blocks-8;++cycle){if(next<blocks)publish();p.feedUntilTarget(N);const double before=p.graphReadPosition();const double stepBegin=1+p.clockTelemetry().correction_ppm*1e-6;require(p.render(N,position,time(position)),"ASRC render/discontinuity");position+=N;const double after=p.graphReadPosition();const auto t=p.clockTelemetry();const double stepEnd=1+t.correction_ppm*1e-6;require(t.resync_count==0,"unannounced ASRC reset");
   writeTap(pcm,p.rendered().readPointers(),channels);positions<<rendered<<','<<before<<','<<after<<','<<source[0].header.stream_epoch<<','<<stepBegin<<','<<stepEnd<<','<<t.correction_ppm<<','<<t.resync_count<<'\n';
   // Retain the actual varying-rate trajectory, rather than assuming a linear
   // position between endpoints. Sequential additions reproduce FP64 rounding.
   const double prefix=pc.targetFrames+asrc::PolyphaseSinc::kTaps+4;
   double nativeRead=before+prefix;
   for(unsigned f=0;f<N;++f){const double read=nativeRead-prefix;nativeRead+=stepBegin+(stepEnd-stepBegin)*double(f)/(N-1);if(f%64)continue;const auto lower=int64_t(std::floor(read));const auto kernel=asrc::PolyphaseSinc::kernelFor(read-lower);
    for(unsigned c=0;c<channels;++c){double expected=0;for(int tap=0;tap<asrc::PolyphaseSinc::kTaps;++tap){const double coefficient=kernel.first[tap]+(kernel.second[tap]-kernel.first[tap])*kernel.blend;expected+=value(lanes[c].asUnsigned(15),lower+asrc::PolyphaseSinc::kFirstOffset+tap)*coefficient;}const double error=expected-p.rendered().readChannel(c)[f];maximum=std::max(maximum,std::abs(error));square+=error*error;++count;}}
   rendered+=N;
  }
  std::cout<<"ASRC domain "<<slot<<" scalar maximum="<<std::setprecision(17)<<maximum<<" RMS="<<std::sqrt(square/count)<<'\n';
  require(count>100&&maximum<1e-8,"ASRC independent scalar coefficient oracle");summary.emplace_back(J::Object{{"domain",slot},{"rateHz",rate},{"samples",J::u64(rendered)},{"oracleSamples",J::u64(count)},{"maximumAbsoluteError",maximum},{"rmsError",std::sqrt(square/count)},{"resyncCount",0},{"hardwareReplay",false}});
 }return summary;
}
int main(int argc,char** argv)try{
 require(argc==5||argc==6,"Usage: EntropyStage020Transfer ACTIVE-copy.entropy existing-fragment.raw active-core.json output-directory [--asrc-only]");auto root=std::filesystem::u8path(argv[4]);std::filesystem::create_directories(root);auto active=loadJson(std::filesystem::u8path(argv[3]));
 control::ProfileStore store;auto profile=store.load(std::filesystem::u8path(argv[1]));active["profileDevices"]=profile.document.at("stage05_devices").at("configuration");std::vector<Recorded> recorded(blocks);std::ifstream inputFile(std::filesystem::u8path(argv[2]),std::ios::binary);inputFile.read(reinterpret_cast<char*>(recorded.data()),sizeof(Recorded)*blocks);require(bool(inputFile)&&inputFile.peek()==EOF,"six-second existing capture size");
 for(unsigned b=0;b<blocks;++b){require(recorded[b].header.frames==N,"recorded block frames");if(b)require(recorded[b].header.block_sequence==recorded[b-1].header.block_sequence+1&&recorded[b].header.first_frame==recorded[b-1].header.first_frame+N&&recorded[b].header.stream_epoch==recorded[0].header.stream_epoch,"recorded frame discontinuity");}
 J::Array cases;std::vector<HeadBlock> baselineHead(blocks);
 auto loadHead=[&](const char* variant){std::ifstream head(root/variant/"post-headroom.f64",std::ios::binary);head.read(reinterpret_cast<char*>(baselineHead.data()),sizeof(HeadBlock)*blocks);require(bool(head)&&head.peek()==EOF,"saved tap size");};
 if(argc==6){const auto option=std::string(argv[5]);require(option=="--asrc-only"||option=="--diagnostic-asrc-only","unknown resume option");if(option=="--asrc-only"){loadHead("active");saveJson(root/"asrc-summary.json",asrcReplay(baselineHead,recorded,active,root));}loadHead("without-manual-lr-ap");saveJson(root/"without-manual-lr-ap"/"asrc-summary.json",asrcReplay(baselineHead,recorded,active,root/"without-manual-lr-ap"));return 0;}
 for(unsigned variant=0;variant<2;++variant){auto caseRoot=root/(variant?"without-manual-lr-ap":"active");std::filesystem::create_directories(caseRoot);auto graph=std::make_unique<engine::AudioGraph>();require(graph->prepare(N),"Graph prepare");graph->enableOriginalMusicAnalysis(true);control::ControlPlane controls(*graph);controls.crossover().importBytes(profile.crossover);controls.replace(profile.parameters);J::Array changes;
  if(variant){for(const auto& p:active.at("telemetry").at("data").at("parameters").asArray()){const auto id=p.at("id").asString();if(id.rfind("source",0)!=0||id=="source")continue;unsigned ch=0;try{ch=unsigned(std::stoul(id.substr(6)));}catch(...){continue;}if(ch<1||ch>16)continue;const int role=int(p.at("value").asNumber());if(role<1||role>10)continue;auto find=[&](const std::string& name){for(const auto& v:active.at("telemetry").at("data").at("parameters").asArray())if(v.at("id").asString()==name)return v.at("value").asNumber();return 0.0;};const auto suffix=std::to_string(ch);if(find("allPass"+suffix)>=.5&&std::abs(find("apFreq"+suffix)-1000)<.01)changes.emplace_back(J::Object{{"id","allPass"+suffix},{"value",0}});}
   if(!changes.empty())controls.execute(J::Object{{"action","parameters"},{"values",changes}});
  }
  engine::PlanarBuffer input,output;input.prepare(13,N);output.prepare(16,N);input.clear(N);
  for(unsigned b=0;b<160;++b){controls.atBlockBoundary();require(graph->process(input.readPointers(),output.writePointers(),N)==ENTROPY_OK,"warmup");controls.afterBlock();}
  require(controls.settled(),"applied state not settled");auto model=std::make_unique<standalone::visual::Configuration>();auto& xo=*Access::crossover(*graph);const auto descriptor=xo.visualizationSource().applied.load();require(!(descriptor&2)&&xo.visualizationSource().copy(descriptor>>2,*model),"applied coefficient snapshot missing/transitioning");auto snapshot=controls.snapshot();saveJson(caseRoot/"coefficients.json",modelJson(*model,snapshot));saveJson(caseRoot/"applied-state.json",snapshot);
  std::ofstream rawInput(caseRoot/"graph-input.f64",std::ios::binary),cross(caseRoot/"post-crossover.f64",std::ios::binary),matrix(caseRoot/"post-matrix.f64",std::ios::binary),head(caseRoot/"post-headroom.f64",std::ios::binary);std::ofstream identity(caseRoot/"identity.csv");identity<<"replayFrame,sourceFirstFrame,epoch,sequence,sourceQpc\n";
  for(unsigned b=0;b<blocks;++b){input.clear(N);for(unsigned c=0;c<11;++c)std::copy_n(recorded[b].samples[c].data(),N,input.channel(c));controls.atBlockBoundary();require(graph->process(input.readPointers(),output.writePointers(),N)==ENTROPY_OK,"recorded Graph replay");controls.afterBlock();writeTap(rawInput,input.readPointers(),13);writeTap(cross,Access::crossoverOutput(*graph).readPointers(),16);writeTap(matrix,Access::matrixOutput(*graph).readPointers(),16);writeTap(head,output.readPointers(),16);identity<<b*N<<','<<recorded[b].header.first_frame<<','<<recorded[b].header.stream_epoch<<','<<recorded[b].header.block_sequence<<','<<recorded[b].header.qpc_timestamp<<'\n';if(!variant)for(unsigned c=0;c<16;++c)std::copy_n(output.readChannel(c),N,baselineHead[b].samples[c].data());}
  if(variant){store.adopt(profile.document);store.save(root/"DIAGNOSTIC-no-manual-LR-AP.entropy",controls,profile.document.at("stage05_devices"));}
  cases.emplace_back(J::Object{{"variant",variant?"WITHOUT MANUAL LR AP":"ACTIVE ORIGINAL"},{"changedParameters",changes},{"blocks",blocks},{"frames",blocks*N},{"sourceContinuity",true}});
 }
 saveJson(root/"replay-summary.json",J::Object{{"cases",cases},{"recordingPerformed",false},{"hardwareOpened",false},{"sourceFrames",blocks*N},{"sourceEpoch",J::u64(recorded[0].header.stream_epoch)}});saveJson(root/"asrc-summary.json",asrcReplay(baselineHead,recorded,active,root));loadHead("without-manual-lr-ap");saveJson(root/"without-manual-lr-ap"/"asrc-summary.json",asrcReplay(baselineHead,recorded,active,root/"without-manual-lr-ap"));std::cout<<"PASS bounded offline taps and scalar ASRC oracle; reused6s; no source/DAC opened\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL Stage020 "<<e.what()<<'\n';return 1;}
