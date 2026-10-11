#include "MusicCinemaFixture.h"
bool read(input::ProgramIngress& in,double* const* p,unsigned mask,input::GraphInputContext& context){auto until=GetTickCount64()+3000;do{if(in.tryRead(p,mask,context))return true;Sleep(1);}while(GetTickCount64()<until);return false;}
int main() try {
 std::array<std::array<double,1024>,13> audio{};std::array<double*,13> planes{};for(unsigned c=0;c<13;++c)planes[c]=audio[c].data();
 Source music(4,11);input::ProgramIngress in;check(in.start(music.endpoint,input::defaultEndpoint(input::Domain::Cinema)+L".absent",false),"independent ingress starts");input::GraphInputContext context;
 check(read(in,planes.data(),1,context),"MUSIC3 received with Cinema absent");check(context.domains==1,"separate context mask");
 check(!memcmp(context.music.session.session_id,music.session.session_id,16),"exact Music session UUID");check(context.music.block.first_frame>=170000,"arbitrary upstream origin retained");
 for(unsigned c=0;c<13;++c)for(unsigned f=0;f<1024;++f){if(c<3)check(!memcmp(&audio[c][f],&music.audio[c][f],8),"Music bits including signed zero and overrange preserved");else check(audio[c][f]==0,"inactive Cinema and reserved heights zero");}
 in.stop();
 // The required input must reconnect to a restarted publisher without
 // restarting Stage3 or issuing a hidden control command upstream.
 {
  auto source=std::make_unique<Source>(4,41);
  input::ProgramIngress recovering;
  check(recovering.start(source->endpoint,input::defaultEndpoint(input::Domain::Cinema)+L".absent",false),"recovery ingress starts");
  check(read(recovering,planes.data(),1,context),"initial recovery PCM");
  const auto oldSession=context.music.session;
  source.reset();
  const auto until=GetTickCount64()+3000;
  while(recovering.telemetry(input::Domain::Music).state!=input::State::Fault&&GetTickCount64()<until)Sleep(1);
  const auto failed=recovering.telemetry(input::Domain::Music);
  check(failed.state==input::State::Fault&&failed.faults>0,"publisher loss retained");
  source=std::make_unique<Source>(4,41,1);
  check(read(recovering,planes.data(),1,context),"required input reconnects after publisher restart without Core restart");
  check(memcmp(oldSession.session_id,context.music.session.session_id,16)!=0,"new publisher UUID adopted");
  check(recovering.telemetry(input::Domain::Music).faults>=failed.faults,"recovery keeps fault history");
  recovering.stop();
 }
 for(unsigned layout:{5u,6u}){Source cinema(layout,layout);check(in.start(music.endpoint,cinema.endpoint,false),"dual input starts");check(read(in,planes.data(),3,context),"independent music and Cinema clocks accepted");
  check(!memcmp(&context.cinema.session,&cinema.session,256),"Cinema session untouched");check(context.music.block.music_session_revision==91&&context.cinema.block.music_session_revision==0,"no fabricated combined header");
  for(unsigned c=0;c<13;++c)for(unsigned f=0;f<1024;++f){const double* expected=c<3?&music.audio[c][f]:(c<3+cinema.session.channels?&cinema.audio[c-3][f]:nullptr);if(expected)check(!memcmp(&audio[c][f],expected,8),"all mapped planes exact");else check(audio[c][f]==0,"reserved planes silent");}
  auto consumed=in.telemetry(input::Domain::Cinema).consumed;for(int i=0;i<150;++i){check(read(in,planes.data(),1,context),"Music progresses while Cinema inactive");}auto t=in.telemetry(input::Domain::Cinema);check(t.received>consumed+50&&t.discarded>0&&t.gaps==0&&t.faults==0,"inactive queue drained without gap or producer blocking");
  check(read(in,planes.data(),2,context),"Cinema alone accepted");in.stop();check(cinema.publisher.dropped()==0,"producer not stalled by consumer lifecycle");
 }
 std::cout<<"PASS exact Music3/Cinema6/8, independent metadata/clocks, inactive drain, passive lifecycle\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}


