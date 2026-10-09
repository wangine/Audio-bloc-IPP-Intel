// Offline analysis tool: passive capture only, never ARM or an upstream command.
#include "Engine/Control/ProfileStore.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include "Transport/StagePublishV2/StagePublishV2.h"
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
using namespace entropy;
static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Recorded {StageBusBlockHeader header{};std::array<std::array<double,1024>,11> samples{};};
struct Persisted {uint32_t magic,version;EntropyHeadroomState state;uint32_t mode,reserved;double gain;};
static std::string base64(const std::vector<unsigned char>& data){
 const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;
 for(size_t i=0;i<data.size();i+=3){unsigned n=unsigned(data[i])<<16;if(i+1<data.size())n|=unsigned(data[i+1])<<8;if(i+2<data.size())n|=data[i+2];out+=alphabet[(n>>18)&63];out+=alphabet[(n>>12)&63];out+=i+1<data.size()?alphabet[(n>>6)&63]:'=';out+=i+2<data.size()?alphabet[n&63]:'=';}return out;
}
static void copyProfile(const control::LoadedProfile& original,unsigned mode,double gain,const std::filesystem::path& path){
 auto document=original.document;Persisted h{};h.magic=0x35524845;h.version=5;h.state=original.parameters.headroom;h.mode=mode;h.gain=gain;
 std::vector<unsigned char> bytes(sizeof(h)+sizeof(original.parameters.timing));std::memcpy(bytes.data(),&h,sizeof(h));std::memcpy(bytes.data()+sizeof(h),&original.parameters.timing,sizeof(original.parameters.timing));
 uint64_t sum=14695981039346656037ULL;for(auto value:bytes)sum=(sum^value)*1099511628211ULL;
 std::ostringstream checksum;checksum<<std::hex<<std::setw(16)<<std::setfill('0')<<sum;
 for(auto& module:document["modules"].asArray())if(module.at("instance_id").asString()=="headroom.main"){
  module["serialized_state"]=base64(bytes);module["state_checksum_fnv1a64"]=checksum.str();}
 std::ofstream file(path,std::ios::binary);file<<ipc::dump(document);check(bool(file),"profile copy write");
}
int main(int argc,char** argv)try{
 check(argc==3||argc==4,"Usage: EntropyStage012Reference <actual.entropy> <results-directory> [explicit AUTO limit dB for comparison copy]");
 const auto root=std::filesystem::u8path(argv[2]);std::filesystem::create_directories(root);
 control::ProfileStore profiles;auto original=profiles.load(std::filesystem::u8path(argv[1]));
 const double autoLimitDb=argc==4?std::stod(argv[3]):original.parameters.manualGainDb;
 constexpr unsigned blocks=1125; // six seconds at the unchanged 192 kHz / 1024 contract
 std::vector<Recorded> recorded(blocks);unsigned count=0;uint64_t prior=0;
 if(std::filesystem::exists(root/"stage2-fragment.raw")){
  std::ifstream raw(root/"stage2-fragment.raw",std::ios::binary);raw.read(reinterpret_cast<char*>(recorded.data()),std::streamsize(recorded.size()*sizeof(Recorded)));check(bool(raw)&&raw.peek()==EOF,"raw capture size mismatch");
 }else{
 publish::Subscriber subscriber;std::string error;check(subscriber.connect(publish::endpointName(),error),error.c_str());
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
 while(count<blocks){check(std::chrono::steady_clock::now()<deadline,"capture timeout");
  std::array<double*,11> planes{};for(unsigned c=0;c<11;++c)planes[c]=recorded[count].samples[c].data();
  const auto result=subscriber.read(recorded[count].header,planes.data());
  if(result==publish::ReadResult::Empty){LARGE_INTEGER q{};QueryPerformanceCounter(&q);subscriber.heartbeat(q.QuadPart);std::this_thread::sleep_for(std::chrono::microseconds(100));continue;}
  check(result==publish::ReadResult::Block,"capture gap/invalid/disconnected; do not hide missing audio");
  check(!prior||recorded[count].header.block_sequence==prior+1,"capture sequence gap");prior=recorded[count].header.block_sequence;++count;
 }
 subscriber.close();
 {std::ofstream raw(root/"stage2-fragment.raw",std::ios::binary);raw.write(reinterpret_cast<const char*>(recorded.data()),std::streamsize(recorded.size()*sizeof(Recorded)));check(bool(raw),"raw capture write");}
 }
 engine::AudioGraph graph;check(graph.prepare(1024),"Graph prepare");control::ControlPlane controls(graph);
 controls.crossover().importBytes(original.crossover);auto upstream=original.parameters;upstream.mode=ENTROPY_HEADROOM_MODE_BIT_PERFECT;controls.replace(upstream);
 engine::PlanarBuffer input,matrix,autoOutput,manualOutput;input.prepare(13,1024);matrix.prepare(16,1024);autoOutput.prepare(16,1024);manualOutput.prepare(16,1024);input.clear(1024);
 for(unsigned warm=0;warm<200;++warm){controls.atBlockBoundary();check(graph.process(input.readPointers(),matrix.writePointers(),1024)==ENTROPY_OK,"Graph warmup");controls.afterBlock();if(warm>32&&controls.settled())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(controls.settled(),"original Crossover settings not settled");
 const auto ledger=controls.appliedState().headroom;check(ledger.active_channel_mask!=0,"no used physical Matrix destinations");
 const bool music=controls.crossover().snapshot().at("program").asUnsigned(1)==0;
 headroom::HeadroomEngine automatic,manual;
 check(automatic.prepare(192000,1024)&&manual.prepare(192000,1024),"reference prepare");
 for(auto* h:{&automatic,&manual})check(h->setState(ledger)==ENTROPY_OK&&h->setTiming(original.parameters.timing)==ENTROPY_OK,"reference state");
 check(automatic.setManualGainDb(autoLimitDb)==ENTROPY_OK&&automatic.setMode(0)==ENTROPY_OK,"AUTO explicit comparison limit");
 constexpr double manualDb=-12;const double scalar=std::pow(10.0,manualDb/20.0);
 check(manual.setManualGainDb(manualDb)==ENTROPY_OK&&manual.setMode(2)==ENTROPY_OK,"MANUAL copy");
 matrix.clear(1024);EntropyHeadroomAnalysisBlockF64 analysis{sizeof(analysis),16,1024,0,matrix.readPointers()};
 EntropyAudioBlockF64 audio{};audio.struct_size=sizeof(audio);audio.input_channels=audio.output_channels=16;audio.frames=1024;audio.inputs=matrix.readPointers();
 const auto warmLimit=unsigned(std::ceil(ledger.transition_ms*192.0/1024.0))+204;
 for(unsigned warm=0;warm<warmLimit;++warm){if(music)check(automatic.analyzeFinal(analysis)==ENTROPY_OK,"AUTO warmup analysis");audio.outputs=autoOutput.writePointers();check(automatic.process(audio)==ENTROPY_OK,"AUTO warmup");audio.outputs=manualOutput.writePointers();check(manual.process(audio)==ENTROPY_OK,"manual warmup");if(warm>1&&!automatic.modeState().transition_active&&!manual.modeState().transition_active)break;}
 check(!automatic.modeState().transition_active&&!manual.modeState().transition_active,"reference ramp did not settle");
 const auto domains=control::LiveSinkWorkers::decode(original.document.at("stage05_devices").at("configuration"));
 std::array<pcm24::Pcm24ModuleEngine,4> pcm;std::array<std::vector<uint8_t>,4> packed;
 for(unsigned mode=0;mode<2;++mode)for(unsigned d=0;d<2;++d)if(domains[d].enabled){const unsigned i=2*mode+d;EntropyPcm24Config c{};c.struct_size=sizeof(c);c.channels=domains[d].channels;c.maximum_frames=1024;c.output_format=domains[d].format;c.reserved_u32=ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT;c.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;c.seed_mode=ENTROPY_PCM24_SEED_DETERMINISTIC_TEST;c.seed=0x12345678;check(pcm[i].configure(c),"PCM reference configuration");packed[i].resize(16*1024*4);}
 std::ofstream csv(root/"gain-by-block.csv");csv.precision(17);csv<<"sequence,pre_gain_peak,auto_gain_end,manual_gain_end,auto_reduction_db\n";
 long double inputEnergy=0,autoEnergy=0,manualEnergy=0,crossEnergy=0;double ratioError=0,minGain=1,maxGain=0,prePeak=0,manualPeak=0,autoPeak=0;uint64_t samples=0;
 // Offline storage permits a second, stable difference pass after RMS matching.
 // Subtracting two large accumulated energies loses precision on MSVC FP64.
 std::vector<std::array<double,2>> compared;compared.reserve(size_t(blocks)*16*1024);
 for(const auto& record:recorded){for(unsigned ch=0;ch<11;++ch)std::copy(record.samples[ch].begin(),record.samples[ch].end(),input.channel(ch));
  engine::GraphControlAccess::crossover(graph)->setProgramInputEpochs(record.header.music_session_revision,record.header.source_generation[0],record.header.source_generation[2],0xffffu);
  controls.atBlockBoundary();check(graph.process(input.readPointers(),matrix.writePointers(),1024)==ENTROPY_OK,"fragment Graph process");controls.afterBlock();
  if(music)check(automatic.analyzeFinal(analysis)==ENTROPY_OK,"fragment analysis");audio.outputs=autoOutput.writePointers();check(automatic.process(audio)==ENTROPY_OK,"AUTO fragment");audio.outputs=manualOutput.writePointers();check(manual.process(audio)==ENTROPY_OK,"MANUAL fragment");
  check(automatic.telemetry().fault_flags==0&&manual.telemetry().fault_flags==0,"reference overload/fault; choose adequate explicit policy, do not hide it");
  for(unsigned ch=0;ch<16;++ch)if(ledger.active_channel_mask&(1u<<ch))for(unsigned f=0;f<1024;++f){const double x=matrix.channel(ch)[f],a=autoOutput.channel(ch)[f],m=manualOutput.channel(ch)[f];
   check(std::isfinite(x)&&std::isfinite(a)&&std::isfinite(m),"nonfinite reference");ratioError=std::max(ratioError,std::abs(m-x*scalar));
   if(std::abs(x)>1e-14){const auto g=a/x;minGain=std::min(minGain,g);maxGain=std::max(maxGain,g);}
   inputEnergy+=x*x;autoEnergy+=a*a;manualEnergy+=m*m;crossEnergy+=a*m;compared.push_back({a,m});++samples;prePeak=std::max(prePeak,std::abs(x));manualPeak=std::max(manualPeak,std::abs(m));autoPeak=std::max(autoPeak,std::abs(a));}
  csv<<record.header.block_sequence<<','<<automatic.telemetry().measured_input_peak<<','<<automatic.telemetry().groups[0].current_gain<<','<<manual.telemetry().groups[0].current_gain<<','<<automatic.telemetry().auto_analysis_reduction_db<<'\n';
  for(unsigned mode=0;mode<2;++mode)for(unsigned d=0;d<2;++d)if(domains[d].enabled){const unsigned i=2*mode+d;std::array<const double*,16> lanes{};for(unsigned c=0;c<domains[d].channels;++c)lanes[c]=(mode?manualOutput:autoOutput).channel(domains[d].lanes[c]);EntropyPcm24BlockF64 b{};b.struct_size=sizeof(b);b.channels=domains[d].channels;b.frames=1024;b.inputs=lanes.data();b.output_interleaved=packed[i].data();b.output_capacity_bytes=unsigned(packed[i].size());check(pcm[i].process(b)==ENTROPY_OK,"original PCM comparison");}
 }
 check(inputEnergy>1e-12&&ratioError<1e-14,"silent or nonconstant manual reference");
 const double scale=std::sqrt(double(manualEnergy/autoEnergy)); // analysis only; never applied to the engine/profile
 long double residualEnergy=0;for(const auto& pair:compared){const double delta=pair[1]-scale*pair[0];residualEnergy+=delta*delta;}
 const double residual=std::sqrt(double(residualEnergy/samples));
 auto a=pcm[0].telemetry(),m=pcm[2].telemetry();a.overrange_sample_count+=pcm[1].telemetry().overrange_sample_count;a.reserved[0]+=pcm[1].telemetry().reserved[0];m.overrange_sample_count+=pcm[3].telemetry().overrange_sample_count;m.reserved[0]+=pcm[3].telemetry().reserved[0];const bool overload=a.overrange_sample_count||m.overrange_sample_count;
 ipc::Value result=ipc::Value::Object{{"status",overload?"FAIL PCM OVERRANGE":"PASS"},{"seconds",6},{"samples",ipc::Value::u64(samples)},{"activeMask",ledger.active_channel_mask},{"originalMode",original.parameters.mode},{"originalManualGainDb",original.parameters.manualGainDb},{"autoCopyLimitDb",autoLimitDb},{"manualCopyGainDb",manualDb},{"manualScalar",scalar},{"manualMaxAbsError",ratioError},{"preGainPeak",prePeak},{"autoPeak",autoPeak},{"manualPeak",manualPeak},{"autoGainMin",minGain},{"autoGainMax",maxGain},{"autoRms",std::sqrt(double(autoEnergy/samples))},{"manualRms",std::sqrt(double(manualEnergy/samples))},{"analysisOnlyAutoLevelScale",scale},{"levelMatchedResidualRms",residual},{"autoPcmOverrange",ipc::Value::u64(a.overrange_sample_count)},{"manualPcmOverrange",ipc::Value::u64(m.overrange_sample_count)},{"autoRailClamps",ipc::Value::u64(a.reserved[0])},{"manualRailClamps",ipc::Value::u64(m.reserved[0])},{"analogLevelMatch","BLOCKED: requires physical level calibration; no DAC opened"},{"truePeak","NOT IMPLEMENTED; before ClockSync interpolation"}};
 std::ofstream(root/"comparison.json")<<ipc::dump(result);
 copyProfile(original,0,autoLimitDb,root/"AUTO-copy.entropy");copyProfile(original,2,manualDb,root/"MANUAL-SAFE-copy.entropy");
 profiles.load(root/"AUTO-copy.entropy");profiles.load(root/"MANUAL-SAFE-copy.entropy");
 std::cout<<ipc::dump(result)<<'\n';return overload?1:0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
