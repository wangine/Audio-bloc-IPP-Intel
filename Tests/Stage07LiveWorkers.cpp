#include "Engine/Egress/LiveSinkWorkers.h"
#include "Engine/Control/LiveEligibility.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
#include <windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <fstream>
#include <cmath>
using namespace entropy;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Observation {std::atomic<unsigned> opens{0},closes{0},submits{0},nonzero{0};std::atomic<int> first{0},second{0};std::atomic<bool> lost{false};std::array<unsigned,16> lanes{};std::atomic<unsigned> mismatches{0},activeChannels{0};};
struct Fake final:egress_host::Sink {
 Observation& o;EntropyDacSinkConfig c{};uint64_t pos=0;bool running=false;
 explicit Fake(Observation& x):o(x){}
 EntropyResult open(const EntropyDacSinkConfig& v) override{c=v;++o.opens;return ENTROPY_OK;}
 EntropyResult start() noexcept override{running=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{
  std::this_thread::sleep_for(std::chrono::microseconds(5333));if(o.lost)return ENTROPY_ERROR_PROCESSING;
  r.frames=c.requested_buffer_frames;r.bytes=r.frames*c.channels*(c.output_format?4:3);
  r.device_position_frames=pos;r.device_qpc_position=10000000+pos*10000000/192000;pos+=r.frames;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{
  ++o.submits;const auto* bytes=static_cast<const unsigned char*>(b.data);const unsigned stride=c.output_format?4:3;int first=0,second=0;
  for(unsigned channel=0;channel<c.channels;++channel){auto* q=bytes+((b.frames-1)*c.channels+channel)*stride;int32_t value;if(stride==4){std::memcpy(&value,q,4);value/=256;}else{value=int(q[0])|(int(q[1])<<8)|(int(q[2])<<16);if(value&0x800000)value|=int(0xff000000u);}if(!channel)first=value;if(channel==1)second=value;
   if(value){o.activeChannels.fetch_or(1u<<channel);const int expected=int(std::nearbyint(.001*(o.lanes[channel]+1)*8388608));if(std::abs(value-expected)>1)++o.mismatches;}}
  o.first=first;o.second=second;if(first||second)++o.nonzero;b.accepted_frames=b.frames;return ENTROPY_OK;
 }
 EntropyDacSinkStatus status() const noexcept override{EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;s.buffer_frames=c.requested_buffer_frames;s.submitted_frames=uint64_t(o.submits.load())*c.requested_buffer_frames;s.device_position_frames=pos;return s;}
 void stop() noexcept override{running=false;}
 void close() noexcept override{++o.closes;}
};
control::LiveDomain config(unsigned slot,unsigned first,unsigned second){control::LiveDomain d;d.enabled=true;d.channels=2;d.format=1;d.endpoint=slot?"fixture-B":"fixture-A";d.logicalId=slot?"DAC_B":"DAC_A";d.lanes[0]=first;d.lanes[1]=second;return d;}
}
int main()try{
 {engine::AudioGraph g;check(g.prepare(1024),"AUTO ARM fixture prepare");
  auto& h=engine::GraphControlAccess::headroom(g);auto ledger=h.state();ledger.groups[0].preventive_reserve_db=3;
  check(h.setState(ledger)==ENTROPY_OK&&h.setManualGainDb(-3)==ENTROPY_OK&&h.setMode(ENTROPY_HEADROOM_MODE_AUTO)==ENTROPY_OK,"AUTO ARM fixture policy");
  engine::PlanarBuffer in,out;in.prepare(16,1024);out.prepare(16,1024);
  EntropyHeadroomAnalysisBlockF64 a{};a.struct_size=sizeof(a);a.channels=16;a.frames=1024;a.inputs=in.readPointers();
  EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.input_channels=b.output_channels=16;b.frames=1024;b.inputs=in.readPointers();b.outputs=out.writePointers();
  auto step=[&](double value){for(unsigned c=0;c<16;++c)std::fill_n(in.channel(c),1024,value);check(h.analyzeFinal(a)==ENTROPY_OK&&h.process(b)==ENTROPY_OK,"AUTO ARM fixture processing");};
  for(unsigned n=0;n<400;++n)step(.1);
  check(control::liveHeadroomReady(g),"settled AUTO fixture rejected");
  for(unsigned n=0;n<10;++n)step(.95);
  auto mode=h.modeState();check(mode.requested_mode==mode.effective_mode&&mode.transition_active,"AUTO attack fixture not active within same mode");
  check(control::liveHeadroomReady(g),"normal AUTO ATTACK incorrectly blocks ARM");
  for(unsigned n=0;n<240;++n)step(.1);
  mode=h.modeState();check(mode.requested_mode==mode.effective_mode&&mode.transition_active,"AUTO release fixture not active within same mode");
  check(control::liveHeadroomReady(g),"normal AUTO RELEASE incorrectly blocks ARM");
  check(h.controlState().manual_gain_db==-3&&h.timing().release_ms==4000,"ARM eligibility changed Headroom controls");
  check(h.setManualGainDb(-12)==ENTROPY_OK&&h.setMode(ENTROPY_HEADROOM_MODE_MANUAL_SAFE)==ENTROPY_OK,"mode switch fixture");step(.1);
  check(!control::liveHeadroomReady(g),"unfinished non-AUTO mode change accepted at ARM");
  for(unsigned n=0;n<100;++n)step(.1);
  check(control::liveHeadroomReady(g),"settled manual fixture rejected");
  check(h.setMode(ENTROPY_HEADROOM_MODE_AUTO)==ENTROPY_OK,"AUTO entry fixture");step(.1);
  mode=h.modeState();check(mode.requested_mode==ENTROPY_HEADROOM_MODE_AUTO&&mode.transition_active,"AUTO entry ramp fixture inactive");
  check(control::liveHeadroomReady(g),"native AUTO entry ramp incorrectly blocks ARM");
 }
 {engine::AudioGraph g;check(g.prepare(1024),"headroom fixture");control::ControlPlane c(g);check(!control::liveHeadroomReady(g),"unprocessed initial mode transition accepted");check(control::liveHeadroomReady(g,false),"native valid unity policy rejected at runtime");
  c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-12}});c.atBlockBoundary();
  engine::PlanarBuffer input,output;input.prepare(13,1024);output.prepare(16,1024);input.clear(1024);
  for(unsigned i=0;i<100;++i){check(g.process(input.readPointers(),output.writePointers(),1024)==ENTROPY_OK,"policy warmup");c.afterBlock();}
  check(control::liveHeadroomReady(g),"explicit valid manual policy rejected");
  auto& native=engine::GraphControlAccess::headroom(g);auto ledger=native.state();auto covered=ledger;ledger.active_channel_mask=0;check(native.setState(ledger)==ENTROPY_OK,"zero mask fixture");check(control::liveHeadroomReady(g),"native valid inactive lanes rejected");ledger.active_channel_mask=1;native.setState(ledger);check(control::liveHeadroomReady(g),"native valid partial active mask rejected");native.setState(covered);

  c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-4}});c.atBlockBoundary();
  for(unsigned i=0;i<100;++i){g.process(input.readPointers(),output.writePointers(),1024);c.afterBlock();}
  check(control::liveHeadroomReady(g),"user manual -4 dB rejected by integration");
  check(native.controlState().manual_gain_db==-4,"integration changed user gain");
  for(double gain:{-4.0,-1.0,0.0,-12.0}){
   c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",gain}});c.atBlockBoundary();
   for(unsigned i=0;i<100;++i){g.process(input.readPointers(),output.writePointers(),1024);c.afterBlock();}
   check(control::liveHeadroomReady(g),"native accepted manual gain rejected by integration");check(native.controlState().manual_gain_db==gain,"manual setting altered");
   for(unsigned ch=0;ch<16;++ch)std::fill_n(output.channel(ch),1024,.25);
   EntropyAudioBlockF64 block{};block.struct_size=sizeof(block);block.input_channels=block.output_channels=16;block.frames=1024;block.inputs=output.readPointers();block.outputs=output.writePointers();
   check(native.process(block)==ENTROPY_OK,"original manual process failed");const double expected=.25*std::pow(10.0,gain/20.0);
   for(unsigned ch=0;ch<16;++ch)for(unsigned n=0;n<1024;++n)check(output.channel(ch)[n]==expected,"user manual gain changed numerically");
  }
  c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",1},{"manualGainDb",0}});c.atBlockBoundary();
  for(unsigned i=0;i<100;++i){g.process(input.readPointers(),output.writePointers(),1024);c.afterBlock();}
  check(control::liveHeadroomReady(g),"original BIT PERFECT rejected by integration");
  for(unsigned ch=0;ch<16;++ch)std::fill_n(output.channel(ch),1024,ch?-0.0:.25);
  EntropyAudioBlockF64 bypass{};bypass.struct_size=sizeof(bypass);bypass.input_channels=bypass.output_channels=16;bypass.frames=1024;bypass.inputs=output.readPointers();bypass.outputs=output.writePointers();check(native.process(bypass)==ENTROPY_OK,"original bit-perfect process failed");
  for(unsigned ch=0;ch<16;++ch)for(unsigned n=0;n<1024;++n){const double expected=ch?-0.0:.25;check(std::memcmp(output.channel(ch)+n,&expected,sizeof(double))==0,"BIT PERFECT signed-zero identity lost");}
  auto invalid=covered;invalid.groups[0].bounded=0;check(native.setState(invalid)!=ENTROPY_OK,"original invalid ledger accepted");check(!control::liveHeadroomReady(g),"original module fault ignored");check(native.setState(covered)==ENTROPY_OK&&native.clearFault()==ENTROPY_OK,"original fault recovery failed");
  // Start AUTO from an actually attenuated state; BIT PERFECT -> unity AUTO
  // can legitimately have no gain ramp in the original module.
  c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-12}});c.atBlockBoundary();
  for(unsigned i=0;i<100;++i){g.process(input.readPointers(),output.writePointers(),1024);c.afterBlock();}
  c.execute(ipc::Value::Object{{"action","headroom-policy"},{"mode",0},{"manualGainDb",-12}});c.atBlockBoundary();g.enableOriginalMusicAnalysis(true);bool sawTransition=false;
  for(unsigned i=0;i<100;++i){for(unsigned ch=0;ch<11;++ch)std::fill_n(input.channel(ch),1024,.8);g.process(input.readPointers(),output.writePointers(),1024);c.afterBlock();if(native.modeState().transition_active){sawTransition=true;check(control::liveHeadroomReady(g,false),"normal AUTO ramp rejected at runtime");}}
  check(sawTransition,"AUTO ramp fixture not active");
  ipc::Value::Array parameters;for(unsigned i=1;i<=16;++i){parameters.emplace_back(ipc::Value::Object{{"id","firLength"+std::to_string(i)},{"value",2}});parameters.emplace_back(ipc::Value::Object{{"id","processMode"+std::to_string(i)},{"value",i==16?0:1}});}ipc::Value current=ipc::Value::Object{{"parameters",parameters}};
  check(control::liveFirAllowed(current),"15 FIR allowed fixture");check(!control::liveCrossoverChangeAllowed(current,ipc::Value::Object{{"action","parameter"},{"id","processMode16"},{"value",1}}),"single8193 restriction bypass");check(!control::liveCrossoverChangeAllowed(current,ipc::Value::Object{{"action","parameters"},{"values",ipc::Value::Array{ipc::Value::Object{{"id","processMode16"},{"value",1}}}}}),"bulk8193 restriction bypass");check(!control::liveCrossoverChangeAllowed(current,ipc::Value::Object{{"action","left-to-right"}}),"left/right8193 restriction bypass");check(!control::liveCrossoverChangeAllowed(current,ipc::Value::Object{{"action","program"},{"program",1}}),"scene8193 restriction bypass");
  auto gate=std::filesystem::temp_directory_path()/"EntropyStage07Test.gate";const std::string hash(64,'A');
  {std::ofstream f(gate);f<<ipc::dump(ipc::Value::Object{{"format","ENTROPY_NATIVE_LIVE_OFFLINE_PASS_V1"},{"audioCoreSHA256",hash},{"ctestPass",true},{"numericRows",96},{"clockRows",8},{"benchmarkRows",80},{"deadlineMisses",0},{"realtimeAuditPass",true}});}
  check(control::liveGateMatches(gate,hash),"valid gate rejected");check(!control::liveGateMatches(gate,std::string(64,'B')),"stale gate accepted");std::filesystem::remove(gate);
 }
 check(host::allocation_audit::selfTest(),"audit positive control");host::allocation_audit::reset();
 std::array<Observation,2> seen;
 control::LiveSinkWorkers w([&](unsigned slot){return std::make_unique<Fake>(seen[slot]);});
 std::array<control::LiveDomain,2> domains{config(0,3,0),config(1,15,2)};
 for(unsigned i=0;i<2;++i)seen[i].lanes=domains[i].lanes;
 check(w.prepare(domains),"prepare dynamic mapping");check(seen[0].opens==0&&seen[1].opens==0,"prepare opened hardware");
 check(w.start(),"explicit ARM");
 std::array<std::array<double,1024>,16> samples;std::array<const double*,16> pointers;
 for(unsigned i=0;i<16;++i){samples[i].fill(.001*(i+1));pointers[i]=samples[i].data();}
 for(unsigned i=0;i<65;++i){{host::allocation_audit::CycleScope scope;host::allocation_audit::audioPhase();w.publish(pointers.data(),1024,true);}std::this_thread::sleep_for(std::chrono::microseconds(5333));}
 auto counts=host::allocation_audit::snapshot();check(!counts.cppNew&&!counts.cppDelete&&!counts.heapAlloc&&!counts.heapFree&&!counts.wait&&!counts.lock&&!counts.io,"Graph publish RT audit failed");
 check(seen[0].nonzero>10&&seen[1].nonzero>10,"all-zero physical payload false PASS");
 check(seen[0].first>seen[0].second*3&&seen[1].first>seen[1].second*5,"explicit lane order lost");
 check(w.armed(),"running state not reported");const auto observed=w.snapshot();for(const auto& domain:observed.asArray())check(domain.at("nonzeroPcmBlocks").asU64()>10,"submitted all-zero payload must not pass");
 seen[1].lost=true;
 for(unsigned i=0;i<15;++i){w.publish(pointers.data(),1024,true);std::this_thread::sleep_for(std::chrono::microseconds(5333));}
 check(!w.armed(),"device loss did not fail group closed");w.stop();check(seen[0].closes>0&&seen[1].closes>0,"owner close missing");
 seen[1].lost=false;check(w.prepare(domains)&&w.start(),"explicit recovery");
 for(unsigned i=0;i<30;++i){w.publish(pointers.data(),1024,true);std::this_thread::sleep_for(std::chrono::microseconds(5333));}
 w.publish(pointers.data(),1024,false);std::this_thread::sleep_for(std::chrono::milliseconds(20));check(!w.armed(),"invalid Headroom did not fail closed");w.stop();
 check(w.prepare(domains)&&w.start(),"starvation recovery");
 for(unsigned i=0;i<25;++i){w.publish(pointers.data(),1024,true);std::this_thread::sleep_for(std::chrono::microseconds(5333));}
 std::this_thread::sleep_for(std::chrono::milliseconds(120));check(!w.armed(),"starvation invisible");w.stop();
 for(unsigned format=0;format<2;++format){for(unsigned slot=0;slot<2;++slot){domains[slot].channels=8;domains[slot].format=format;seen[slot].activeChannels=0;seen[slot].mismatches=0;for(unsigned c=0;c<8;++c)domains[slot].lanes[c]=slot*8+c;seen[slot].lanes=domains[slot].lanes;}
  check(w.prepare(domains)&&w.start(),"all16 formats ARM");for(unsigned i=0;i<40;++i){w.publish(pointers.data(),1024,true);std::this_thread::sleep_for(std::chrono::microseconds(5333));}
  check(w.armed(),"all16 formats unexpectedly faulted");w.stop();for(auto& x:seen)check(x.activeChannels==255&&x.mismatches==0,"all16 PCM oracle/active channel failure");
 }
 domains[1].lanes[0]=domains[0].lanes[0];check(!w.prepare(domains),"duplicate lane accepted");
 std::puts("PASS Stage07 separate owner threads, explicit dynamic lanes, active PCM, loss, starvation, policy, stop/recovery; no hardware");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL Stage07 %s\n",e.what());return 1;}
