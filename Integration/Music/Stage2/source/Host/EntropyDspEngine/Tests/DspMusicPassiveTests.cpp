#include "MusicTestSupport.h"
#include "DspStageRuntime.h"
int main(){
 using namespace entropy::publish;std::string error;auto endpoint=musicEndpoint(L".source");
 Publisher source,output;auto s=session(3);s.producer_pid=GetCurrentProcessId();
 CHECK(source.start(endpoint,error));source.setSession(s);CHECK(output.start(endpoint+L".out",error));
 DspStageRuntime runtime;runtime.setPublisher(&output);
 CHECK(musicConnect(source,[&]{return runtime.attachPassive(endpoint,error);}));
 CHECK(runtime.outputSession().channels==3 && runtime.outputSession().layout_id==4);
 Subscriber fast,slow;CHECK(musicConnect(output,[&]{return fast.connect(endpoint+L".out",error);}));
 CHECK(musicConnect(output,[&]{return slow.connect(endpoint+L".out",error);}));
 Planes<2> in;Planes<3> out;
 for(uint64_t sequence=71;sequence<105;sequence++){
  for(unsigned f=0;f<1024;f++){uint64_t value=f%3==0?0x8000000000000000ULL:f%3==1?0x7ff8000000000000ULL+f:0x3fe0000000000000ULL+f;std::memcpy(&in.data[0][f],&value,8);in.data[1][f]=-.123+f*.00001;}
  auto b=musicHeader(s,sequence);b.stream_epoch=7;b.discontinuity_mask=1;
  source.publish(b,in.in.data());CHECK(runtime.step(qpc())==DspStepResult::Published);
  StageBusBlockHeader received{};CHECK(fast.read(received,out.out.data())==ReadResult::Block);
  CHECK(!std::memcmp(&b,&received,sizeof(b)));
  CHECK(!std::memcmp(in.in[0],out.in[0],8192) && !std::memcmp(in.in[1],out.in[1],8192));
  for(auto sample:out.data[2])CHECK(bits(sample)==0);
 }
 StageBusBlockHeader received{};CHECK(slow.read(received,out.out.data())==ReadResult::Gap);
 CHECK(output.dropped()>0 && runtime.diagnostics().blocksOut==34);
 fast.close();slow.close();output.service();
 CHECK(musicConnect(output,[&]{return fast.connect(endpoint+L".out",error);}));
 auto b=musicHeader(s,105);b.stream_epoch=7;source.publish(b,in.in.data());CHECK(runtime.step(qpc())==DspStepResult::Published);
 CHECK(fast.read(received,out.out.data())==ReadResult::Block && received.block_sequence==105);
 fast.close();runtime.stop();runtime.setPublisher(nullptr);output.stop();source.stop();
 // A declared legacy layout is valid transport metadata but never valid Stage2 MUSIC ingress.
 CHECK(source.start(endpoint,error));auto legacy=session(MIX_CINEMA10_V1);legacy.producer_pid=GetCurrentProcessId();source.setSession(legacy);
 CHECK(!musicConnect(source,[&]{return runtime.attachPassive(endpoint,error);}));
 CHECK(error.find("MUSIC")!=std::string::npos);
 source.stop();
 // Subscription gap affects this runtime only; publisher and its identity survive reconnect.
 CHECK(source.start(endpoint,error));source.setSession(s);CHECK(musicConnect(source,[&]{return runtime.attachPassive(endpoint,error);}));
 for(uint64_t seq=200;seq<220;seq++){b=musicHeader(s,seq);source.publish(b,in.in.data());}
 CHECK(runtime.step(qpc())==DspStepResult::Fault);CHECK(runtime.diagnostics().missing>0);
 CHECK(musicConnect(source,[&]{return runtime.attachPassive(endpoint,error);}));
 b=musicHeader(s,220);source.publish(b,in.in.data());CHECK(runtime.step(qpc())==DspStepResult::Published);
 CHECK(!std::memcmp(runtime.outputSession().session_id,s.session_id,16));
 runtime.stop();source.stop();
 CHECK(endpointName().find(L"Music.Dsp.V2")!=std::wstring::npos);
 CHECK(sourceEndpointName().find(L"Music.Source.V2")!=std::wstring::npos);
 std::cout<<"PASS stereo MUSIC passive bypass bits/headers, epoch, slow consumer, reconnect, legacy rejection, local gap\n";
}
