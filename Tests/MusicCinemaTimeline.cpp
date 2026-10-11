#include "MusicCinemaFixture.h"
#include "Engine/AudioScheduler.h"
using J=ipc::Value;
namespace {
// Only the physical endpoint is simulated; ingress, Graph, ASRC and sink owners are real.
struct Endpoint final:egress_host::Sink {
 EntropyDacSinkConfig config{};uint64_t position=0;bool running=false;
 EntropyResult open(const EntropyDacSinkConfig& c) override{config=c;return ENTROPY_OK;}
 EntropyResult start() noexcept override{running=true;return ENTROPY_OK;}
 EntropyResult wait(EntropyDacSinkBufferRequest& r) noexcept override{
  Sleep(5);r.frames=1024;r.bytes=1024*config.channels*(config.output_format?4:3);
  r.device_position_frames=position;r.device_qpc_position=10000000+position*10000000/192000;return ENTROPY_OK;
 }
 EntropyResult submit(EntropyDacSinkPcmBlock& b) noexcept override{position+=b.frames;b.accepted_frames=b.frames;return ENTROPY_OK;}
 EntropyDacSinkStatus status() const noexcept override{EntropyDacSinkStatus s{};s.struct_size=sizeof(s);s.buffer_frames=1024;s.state=running?ENTROPY_DAC_SINK_RUNNING:ENTROPY_DAC_SINK_OPEN;return s;}
 void stop() noexcept override{running=false;}
 void close() noexcept override{}
};
template<class F>bool await(F f){const auto until=GetTickCount64()+3000;while(!f()&&GetTickCount64()<until)Sleep(1);return f();}
}
int main()try{
 Source music(4,81,0,.000001),cinema(6,82,0,.000001);
 input::ProgramIngress ingress;check(ingress.start(music.endpoint,cinema.endpoint,false),"ingress starts");
 engine::AudioGraph graph;check(graph.prepare(1024),"Graph prepared");
 check(graph.setOfflineParameter("cinemaSource",0)&&graph.applyOfflineParameters(true),"initial MUSIC program");
 control::ControlPlane controls(graph);controls.execute(J::Object{{"action","headroom-policy"},{"mode",2},{"manualGainDb",-6}});
 input::ProgramSelection selection;selection.quiescent(0);
 control::LiveSinkWorkers sinks([](unsigned){return std::make_unique<Endpoint>();},false);
 control::AudioScheduler scheduler(graph,controls,ingress,selection,&sinks);scheduler.start();
 std::array<control::LiveDomain,2> domains{};for(unsigned i=0;i<2;++i){auto& d=domains[i];d.enabled=true;d.channels=1;d.format=1;d.endpoint=i?"fixture-B":"fixture-A";d.logicalId=i?"DAC_B":"DAC_A";d.lanes[0]=i;}
 check(await([&]{return scheduler.telemetry().blocks>16;}),"MUSIC running");
 check(sinks.prepare(domains)&&sinks.start(),"DAC owners start");
 check(await([&]{return sinks.armed();}),"DAC owners armed");
 const auto revision=ingress.telemetry(input::Domain::Music).block.music_session_revision;
 music.musicRevision.fetch_add(1);music.generation=24;
 check(await([&]{return ingress.telemetry(input::Domain::Music).block.music_session_revision!=revision;}),"source epoch change consumed");
 auto blocks=scheduler.telemetry().blocks;
 check(await([&]{return scheduler.telemetry().blocks>=blocks+32||sinks.faulted();}),"Graph advances after source change");
 check(!sinks.faulted()&&sinks.armed(),"source generation incorrectly invalidated Graph-to-DAC clock");
 for(unsigned program:{1u,0u}){
  std::string error;check(selection.request(program,ingress,error),"target program available");
  controls.execute(J::Object{{"action","parameter"},{"id","cinemaSource"},{"value",program}});selection.accepted(program);
  check(await([&]{return selection.requiredDomains()==(1u<<program)||sinks.faulted();}),"native crossfade settles");
  blocks=scheduler.telemetry().blocks;
  check(await([&]{return scheduler.telemetry().blocks>=blocks+32||sinks.faulted();}),"selected program advances");
  check(!sinks.faulted()&&sinks.armed(),"program switch incorrectly invalidated Graph-to-DAC clock");
 }
 sinks.stop();scheduler.stop();ingress.stop();
 std::cout<<"PASS MUSIC source generation/revision and MUSIC-CINEMA-MUSIC switches retain continuous DAC clock; real Graph and ASRC, no hardware\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
