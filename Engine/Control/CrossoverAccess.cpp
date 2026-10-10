#include <windows.h>
#include "CrossoverAccess.h"
#include "GraphControlAccess.h"
// Generated from the immutable original compatibility/control implementation.
// The two mechanical changes are ownership type and exported symbol name.
#include "OriginalCrossoverControl.inc"
#include <stdexcept>
namespace entropy::control {
using J=ipc::Value;
struct CrossoverAccess::Impl {CrossoverState state;std::vector<std::string> ids;std::array<std::atomic<int>,16> roles{};std::array<std::atomic<bool>,16> active{};std::array<std::atomic<float>,entropy::standalone::parameterCount> published{};std::array<float,entropy::standalone::parameterCount> owner{};std::atomic<uint64_t> revision{0};uint64_t lastAudible=0;Engine::ControlCommandTag lastAudibleTag{};
 int capturedRole(int channel) const noexcept {using entropy::standalone::parameterIndex;return int(std::lround(owner[(owner[parameterIndex("cinemaSource")]>=.5f?parameterIndex("cinemaRole1"):parameterIndex("source1"))+channel]));}
 bool capturedActive(int role) const noexcept {
  using entropy::standalone::parameterIndex;
  const auto lr=std::clamp(int(std::lround(owner[parameterIndex("lrWays")])),1,5);
  const auto center=std::clamp(int(std::lround(owner[parameterIndex("centerWays")])),1,2);
  if(role>=1&&role<=5)return role<=lr;if(role>=6&&role<=10)return role-5<=lr;
  if(role>=11&&role<=15)return role-10<=center;if(role==16||role==17)return true;
  if(role==18)return int(std::lround(owner[parameterIndex("subSpeakers")]))>=2;
  if(role==19)return int(std::lround(owner[parameterIndex("scoopSpeakers")]))>=2;
  return role>=20&&role<=25;
 }
 void publish(){revision.fetch_add(1,std::memory_order_acq_rel);for(int i=0;i<entropy::standalone::parameterCount;++i){auto* raw=state.engine->getAPVTS().getRawParameterValue(entropy::standalone::parameterIds[i]);published[i].store(raw?raw->load():0,std::memory_order_relaxed);}revision.fetch_add(1,std::memory_order_release);}
};
static void require(EntropyResult r){if(r!=ENTROPY_OK)throw std::invalid_argument("Crossover rejected command: "+std::to_string(r));}
CrossoverAccess::CrossoverAccess(engine::AudioGraph& graph):impl_(std::make_unique<Impl>()){
 impl_->state.engine=engine::GraphControlAccess::crossover(graph);if(!impl_->state.engine)throw std::invalid_argument("Graph not prepared");
 impl_->state.viewWorker=std::make_unique<entropy::standalone::visual::Worker>(*impl_->state.engine);
 impl_->state.prepared=true;impl_->state.prepare.struct_size=sizeof(EntropyPrepareInfo);impl_->state.prepare.sample_rate=192000;impl_->state.prepare.maximum_frames=engine::GraphControlAccess::maximumFrames(graph);impl_->state.prepare.input_channels=13;impl_->state.prepare.output_channels=16;
 for(const auto& p:Engine::createParameterLayout())impl_->ids.emplace_back(p->paramID.toRawUTF8());
 impl_->publish();for(int i=0;i<entropy::standalone::parameterCount;++i)impl_->owner[i]=impl_->published[i].load();
 captureAfterBlock();
}
CrossoverAccess::~CrossoverAccess()=default;
bool CrossoverAccess::settled() const noexcept {auto s=impl_->state.engine->controlCommandStatus();return s.applied.sequence>=impl_->lastAudible;}
void CrossoverAccess::resetQuiescent(){
 auto& engine=*impl_->state.engine;const auto status=engine.controlCommandStatus();
 const bool pending=status.applied.sequence<impl_->lastAudible;
 engine.prepareToPlay(192000,int(impl_->state.prepare.maximum_frames));
 // Native reset retires coefficient slots. Recompile the same accepted identity;
 // only a subsequently processed real block can acknowledge it as applied.
 if(pending&&!engine.publishControlSnapshot(impl_->lastAudibleTag))throw std::runtime_error("Crossover requeue after local reset failed");
}
bool CrossoverAccess::preparedFinite() const noexcept {return impl_->state.engine->preparedCoefficientsFinite();}
J CrossoverAccess::preparationCache() const {auto counts=impl_->state.engine->preparationCacheCounts();return J::Object{{"builds",J::u64(counts[0])},{"hits",J::u64(counts[1])},{"finite",preparedFinite()}};}
J CrossoverAccess::preparationParameters(const J& change) const {
 J::Array parameters;for(const auto& id:impl_->ids){double value=0;require(getParameter(reinterpret_cast<EntropyModuleHandle>(&impl_->state),id.c_str(),&value));parameters.emplace_back(J::Object{{"id",id},{"value",value}});}
 auto set=[&](const std::string& id,const J& value){for(auto& p:parameters)if(p.at("id").asString()==id)p["value"]=value;};
 bool sceneChange=false;int target=0,current=impl_->state.engine->getAPVTS().getRawParameterValue("cinemaSource")->load()>=.5f?1:0;
 if(!change.isNull()){
  auto action=change.at("action").asString();
  auto entry=[&](const J& p){set(p.at("id").asString(),p.at("value"));if(p.at("id").asString()=="cinemaSource"){target=p.at("value").asUnsigned(1);sceneChange=target!=current;}};
  if(action=="program"){target=change.at("program").asUnsigned(1);sceneChange=target!=current;set("cinemaSource",target);}
  else if(action=="parameter")entry(change);
  else if(action=="parameters")for(const auto& p:change.at("values").asArray())entry(p);
 }
 if(sceneChange){auto bytes=exportBytes();auto xml=entropy::native::getXmlFromBinary(bytes.data(),int(bytes.size()));if(!xml)throw std::runtime_error("Scene state unavailable");auto tree=entropy::native::XmlNode::fromXml(*xml);
  for(int n=0;n<tree.getNumChildren();++n){auto node=tree.getChild(n);if(!node.hasType(target?"CINEMA_CROSSOVER_SCENE":"MUSIC_CROSSOVER_SCENE"))continue;
   for(const auto& id:impl_->ids)if(node.hasProperty(id.c_str())){auto text=node.getProperty(id.c_str()).toString().toStdString();float value=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);if(parsed.ec!=std::errc{}||!std::isfinite(value))throw std::invalid_argument("Nonfinite saved scene");set(id,double(value));}
  }
 }
 return J::Object{{"parameters",std::move(parameters)}};
}
J CrossoverAccess::metadata() const {
 J::Array a;
 for(const auto& id:impl_->ids){auto* p=impl_->state.engine->getAPVTS().getParameter(id.c_str());const auto& r=p->getNormalisableRange();unsigned kind=0,choices=0;if(dynamic_cast<entropy::native::AudioParameterBool*>(p))kind=1;else if(auto* c=dynamic_cast<entropy::native::AudioParameterChoice*>(p)){kind=2;choices=unsigned(c->choices.size());}
 a.emplace_back(J::Object{{"id",id},{"name",p->getName(95).toRawUTF8()},{"label",p->getName(95).toRawUTF8()},{"min",r.start},{"max",r.end},{"default",p->convertFrom0to1(p->getDefaultValue())},{"kind",kind},{"choices",choices}});}
 return J::Object{{"parameters",std::move(a)}};
}
J CrossoverAccess::snapshot() const {
 J::Array parameters,outputs;auto names=Engine::roleNames();
 for(const auto& id:impl_->ids){double v=0;require(getParameter(reinterpret_cast<EntropyModuleHandle>(&impl_->state),id.c_str(),&v));parameters.emplace_back(J::Object{{"id",id},{"value",v}});}
 for(uint32_t i=0;i<16;++i){auto role=unsigned(impl_->roles[i].load());bool active=impl_->active[i].load();uint32_t semantic=0,way=0;outputRole(role,semantic,way);std::string label=active?"CH"+std::to_string(i+1)+" "+names[int(role)].toRawUTF8():"";auto delay=impl_->state.engine->getFractionalDelayStatus(i);outputs.emplace_back(J::Object{{"active",active},{"label",label},{"role",role},{"semantic",semantic},{"way",way},{"fractionalDelayOrder",delay&0xffu},{"fractionalDelayTransition",bool(delay&0x100u)}});}
 uint32_t program=0;getProgram(reinterpret_cast<EntropyModuleHandle>(&impl_->state),&program);auto status=impl_->state.engine->controlCommandStatus();
 J::Array peaks;for(int i=0;i<16;++i)peaks.emplace_back(impl_->state.engine->getOutputPeak(i));
 J::Array music,cinema,protection,response,frequency,spectrum;for(int i=0;i<3;++i)music.emplace_back(impl_->state.engine->getMainInputPeak(i));for(int i=0;i<10;++i)cinema.emplace_back(impl_->state.engine->getCinemaInputPeak(i));for(int i=0;i<16;++i)protection.emplace_back(unsigned(impl_->state.engine->getProtectionState(i)));
 const auto view=impl_->state.viewWorker->snapshot();J::Array responseFrequency,phase;
 for(const auto& row:view.responseDb){J::Array values;for(auto point:row)values.emplace_back(point);response.emplace_back(std::move(values));}
 for(const auto& row:view.responsePhaseRadians){J::Array values;for(auto point:row)values.emplace_back(point);phase.emplace_back(std::move(values));}
 for(auto point:view.responseFrequencyHz)responseFrequency.emplace_back(point);for(auto p:view.frequency)frequency.emplace_back(p);for(auto p:view.spectrum)spectrum.emplace_back(p);
 LARGE_INTEGER qpf{};QueryPerformanceFrequency(&qpf);

 EntropyCrossoverProtectionSummary summary{};summary.struct_size=sizeof(summary);require(getProtectionSummary(reinterpret_cast<EntropyModuleHandle>(&impl_->state),&summary));
 return J::Object{{"parameters",std::move(parameters)},{"outputs",std::move(outputs)},{"program",program},{"blockSize",unsigned(impl_->state.engine->getRuntimeBlockSize())},{"crossoverAccepted",J::u64(status.accepted.sequence)},{"crossoverApplied",J::u64(status.applied.sequence)},{"crossoverFailed",J::u64(status.failed.sequence)},{"crossoverReason",status.reason},{"crossoverKind",status.accepted.kind},{"crossoverPeaks",std::move(peaks)},{"musicPeaks",std::move(music)},{"cinemaPeaks",std::move(cinema)},{"firLatency",unsigned(impl_->state.engine->getReportedFirLatencySamples())},{"protectionState",std::move(protection)},{"protectionEnabled",summary.enabled_mask},{"protectionWarning",summary.warning_mask},{"protectionTripped",summary.tripped_mask},{"view",J::Object{{"version",2},{"units","dBFS peak (bin amplitude, Hann sum N)"},{"N",16384},{"points",256},{"response",std::move(response)},{"responseFrequencyHz",std::move(responseFrequency)},{"responsePhaseRadians",std::move(phase)},{"responseAvailable",view.responseAvailable},{"responseState",view.transition?"TRANSITION":view.responseAvailable?"APPLIED":"UNAVAILABLE"},{"responseGeneration",J::u64(view.generation)},{"responseBuilds",J::u64(view.responseBuilds)},{"frequency",std::move(frequency)},{"spectrum",std::move(spectrum)},{"spectrumAvailable",view.spectrumAvailable},{"state",view.stale?"STALE":view.capture.transition?"TRANSITION":"LIVE"},{"monitor",view.capture.channel},{"channel",view.capture.channel},{"program",view.capture.program},{"generation",J::u64(view.capture.generation)},{"frameSequence",J::u64(view.capture.frameSequence)},{"captureQpc",J::u64(view.capture.captureQpc)},{"qpcFrequency",double(qpf.QuadPart)},{"sampleRate",view.capture.sampleRate}}}};
}
void CrossoverAccess::execute(const J& request,uint64_t session,uint64_t sequence){
 const auto action=request.at("action").asString();auto& s=impl_->state;
 std::vector<std::pair<std::string,double>> values;
 if(action=="parameter")values.emplace_back(request.at("id").asString(),request.at("value").asNumber());
 else if(action=="parameters")for(const auto& v:request.at("values").asArray())values.emplace_back(v.at("id").asString(),v.at("value").asNumber());
 else if(action=="program")values.emplace_back("cinemaSource",request.at("program").asUnsigned(1));
 else if(action=="apply-side-delay"||action=="left-to-right"){
  // Same role selection and 15 common + 2 scene controls as original R10NativeControls.
  bool cinema=request.at("cinema").asBool();const char* rolePrefix=cinema?"cinemaRole":"source";const char* delayPrefix=cinema?"cinemaDelayMs":"musicDelayMs";
  auto read=[&](const std::string& id){double v=0;require(getParameter(reinterpret_cast<EntropyModuleHandle>(&s),id.c_str(),&v));return v;};
  int ways=std::clamp(int(std::lround(read("lrWays"))),1,5);
  auto channelForRole=[&](int wanted){for(int ch=1;ch<=16;++ch)if(int(std::lround(read(std::string(rolePrefix)+std::to_string(ch))))==wanted)return ch;return -1;};
  if(action=="apply-side-delay"){int first=request.at("right").asBool()?6:1;for(int ch=1;ch<=16;++ch){int role=int(std::lround(read(std::string(rolePrefix)+std::to_string(ch))));if(role>=first&&role<first+ways)values.emplace_back(std::string(delayPrefix)+std::to_string(ch),request.at("value").asNumber());}if(values.empty())throw std::invalid_argument("No output for selected side");}
  else {const std::array<const char*,16> common{{"hpOn","hpHz","lpOn","lpHz","filterType","processMode","firLength","allPass","apFreq","apQ","polarity","protectorOn","safetyMinHz","safetyMaxHz","tripHoldMs","fractionalDelayMode"}};for(int way=1;way<=ways;++way){int left=channelForRole(way),right=channelForRole(5+way);if(left<1||right<1)throw std::invalid_argument("Missing left/right way");for(auto prefix:common)values.emplace_back(std::string(prefix)+std::to_string(right),read(std::string(prefix)+std::to_string(left)));for(auto prefix:{cinema?"cinemaGainDb":"musicGainDb",delayPrefix})values.emplace_back(std::string(prefix)+std::to_string(right),read(std::string(prefix)+std::to_string(left)));}}
 }
 for(const auto& [id,value]:values){auto* p=s.engine->getAPVTS().getParameter(id.c_str());if(!p)throw std::invalid_argument("Unknown parameter: "+id);
 const auto& r=p->getNormalisableRange();if(!std::isfinite(value)||value<r.start||value>r.end||((dynamic_cast<entropy::native::AudioParameterChoice*>(p)||dynamic_cast<entropy::native::AudioParameterBool*>(p))&&std::floor(value)!=value))throw std::invalid_argument("Parameter outside native range: "+id);}
 uint32_t kind=values.size()>1?ENTROPY_CROSSOVER_COMMAND_BULK:ENTROPY_CROSSOVER_COMMAND_PARAMETER;
 if(action=="gesture")kind=request.at("begin").asBool()?ENTROPY_CROSSOVER_COMMAND_GESTURE_BEGIN:ENTROPY_CROSSOVER_COMMAND_GESTURE_END;
 else if(action=="monitor"||action=="reset-protection"||action=="reset-all-protection")kind=ENTROPY_CROSSOVER_COMMAND_CONTROL_ONLY;
 else if(values.empty())throw std::invalid_argument("Unsupported Crossover action");
 EntropyCrossoverCommandIdV1 id{};id.struct_size=sizeof(id);id.extension_version=ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;id.session=session;id.sequence=sequence;
 require(beginControlCommand(reinterpret_cast<EntropyModuleHandle>(&s),&id,kind));
 try{
  if(action=="gesture")require(request.at("begin").asBool()?beginParameterGesture(reinterpret_cast<EntropyModuleHandle>(&s),request.at("id").asString().c_str()):endParameterGesture(reinterpret_cast<EntropyModuleHandle>(&s),request.at("id").asString().c_str()));
 else if(action=="monitor"){auto channel=request.at("channel").asNumber();if(channel<-1||channel>15||std::floor(channel)!=channel)throw std::invalid_argument("Invalid monitor channel");require(requestSpectrumChannel(reinterpret_cast<EntropyModuleHandle>(&s),int(channel)));}
  else if(action=="reset-protection")require(resetProtectionTrip(reinterpret_cast<EntropyModuleHandle>(&s),request.at("channel").asUnsigned(15)));
  else if(action=="reset-all-protection")require(resetAllProtectionTrips(reinterpret_cast<EntropyModuleHandle>(&s)));
  else for(const auto& [key,value]:values)require(setParameter(reinterpret_cast<EntropyModuleHandle>(&s),key.c_str(),value));
  require(commitControlCommand(reinterpret_cast<EntropyModuleHandle>(&s)));
  if(!values.empty()){impl_->lastAudible=sequence;impl_->lastAudibleTag=s.engine->controlCommandStatus().accepted;}
  impl_->publish();
 }catch(...){if(s.controlTransactionOpen)cancelControlCommand(reinterpret_cast<EntropyModuleHandle>(&s),ENTROPY_CROSSOVER_COMMAND_REASON_INTERNAL_ERROR);throw;}
}
std::vector<unsigned char> CrossoverAccess::exportBytes() const {uint32_t n=0;require(exportState(reinterpret_cast<EntropyModuleHandle>(&impl_->state),nullptr,&n));std::vector<unsigned char> b(n);require(exportState(reinterpret_cast<EntropyModuleHandle>(&impl_->state),b.data(),&n));return b;}
void CrossoverAccess::importBytes(const std::vector<unsigned char>& b){
 // Validate the original scalar representation without changing its migration/math.
 auto xml=entropy::native::getXmlFromBinary(b.data(),int(b.size()));if(!xml)throw std::invalid_argument("Invalid Crossover XML");auto tree=entropy::native::XmlNode::fromXml(*xml);
 auto validate=[&](const entropy::native::String& id,const entropy::native::Property& property){auto* parameter=impl_->state.engine->getAPVTS().getParameter(id);if(!parameter)return;auto text=property.toString().toStdString();float value=0;auto result=std::from_chars(text.data(),text.data()+text.size(),value);auto& range=parameter->getNormalisableRange();if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||!std::isfinite(value)||value<range.start||value>range.end||((dynamic_cast<entropy::native::AudioParameterChoice*>(parameter)||dynamic_cast<entropy::native::AudioParameterBool*>(parameter))&&std::floor(value)!=value))throw std::invalid_argument("Profile parameter outside native range: "+id.toStdString());};
 auto validateProperties=[&](const entropy::native::XmlNode& node){for(const auto& id:impl_->ids)if(node.hasProperty(id.c_str()))validate(id.c_str(),node.getProperty(id.c_str()));};
 for(int i=0;i<tree.getNumChildren();++i){auto child=tree.getChild(i);if(child.hasType("PARAM"))validate(child.getProperty("id").toString(),child.getProperty("value"));else if(child.hasType("MUSIC_CROSSOVER_SCENE")||child.hasType("CINEMA_CROSSOVER_SCENE")||child.hasType("MANUAL_SAVE_ALL"))validateProperties(child);else if(child.hasType("MANUAL_SNAPSHOTS"))for(int n=0;n<child.getNumChildren();++n)validateProperties(child.getChild(n));}
 require(importState(reinterpret_cast<EntropyModuleHandle>(&impl_->state),b.data(),uint32_t(b.size())));impl_->publish();impl_->lastAudible=0;}
void CrossoverAccess::captureBeforeBlock() noexcept {
 std::array<float,entropy::standalone::parameterCount> candidate{};for(unsigned attempt=0;attempt<4;++attempt){auto begin=impl_->revision.load(std::memory_order_acquire);if(begin&1)continue;for(int i=0;i<entropy::standalone::parameterCount;++i)candidate[i]=impl_->published[i].load(std::memory_order_relaxed);std::atomic_thread_fence(std::memory_order_acquire);if(begin==impl_->revision.load(std::memory_order_acquire)){impl_->owner=candidate;break;}}
 impl_->state.engine->setHostBlockParameterView(impl_->owner.data());
}
unsigned CrossoverAccess::capturedSourceMask() const noexcept {unsigned mask=0;for(int i=0;i<16;++i)if(impl_->capturedActive(impl_->capturedRole(i)))mask|=1u<<i;return mask;}
void CrossoverAccess::captureAfterBlock() noexcept {for(int i=0;i<16;++i){const auto role=impl_->capturedRole(i);impl_->roles[i].store(role,std::memory_order_relaxed);impl_->active[i].store(impl_->capturedActive(role),std::memory_order_relaxed);}impl_->state.engine->setHostBlockParameterView(nullptr);}
}
