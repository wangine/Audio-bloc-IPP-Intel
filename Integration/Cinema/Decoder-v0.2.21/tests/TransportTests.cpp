#include "CinemaOutput.h"
#include "StagePublishV2.h"
#include "StageBusValidation.h"
#include <windows.h>
#include <array>
#include <future>
#include <iostream>
#include <iomanip>
#include <stdexcept>
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
bool connect(entropy::publish::Subscriber& r,const std::wstring& endpoint){std::string error;for(int i=0;i<20;i++){if(r.connect(endpoint,error))return true;Sleep(10);}std::cerr<<error<<'\n';return false;}
int main(){try{for(unsigned channels:{6u,8u}){
 const auto endpoint=CinemaOutput::defaultEndpoint()+L".IsolatedTest."+std::to_wstring(GetCurrentProcessId());CinemaOutput output;std::string error;require(output.start(channels,endpoint,error),"start");entropy::publish::Subscriber reader;require(connect(reader,endpoint),"connect");require(reader.session().channels==channels,"channels");require(entropy::stage::validateSession(reader.session(),reader.session().mapping_bytes)==entropy::stage::StageBusValidationResult::Ok,"session");
 std::array<std::array<double,1024>,8> audio{};std::array<double*,8> planes{};for(unsigned c=0;c<8;c++)planes[c]=audio[c].data();StageBusBlockHeader b{};
 for(int i=0;i<4096;i++)output.submit({});unsigned received=0;const auto end=GetTickCount64()+1000;uint64_t epoch=0;
 while(GetTickCount64()<end&&received<14){LARGE_INTEGER q{};QueryPerformanceCounter(&q);reader.heartbeat(q.QuadPart);auto result=reader.read(b,planes.data());if(result==entropy::publish::ReadResult::Block){require(b.validity_mask==4&&b.source_rate[2]==48000,"metadata");require(b.first_frame==(b.block_sequence-1)*1024,"counter");require(b.source_generation[2]==b.stream_epoch,"epoch");epoch=b.stream_epoch;for(unsigned c=0;c<channels;c++)for(double v:audio[c])require(v==0,"valid silence PCM");received++;}else require(result==entropy::publish::ReadResult::Empty,"gap");Sleep(1);}
 require(received>=14,"valid silence output");output.invalidate();Sleep(30);reader.close();require(connect(reader,endpoint),"reconnect");std::array<float,8> frame{};for(unsigned c=0;c<channels;c++)frame[c]=float(.01*(c+1));for(int i=0;i<4096;i++)output.submit(frame);bool valid=false;auto deadline=GetTickCount64()+1000;
 while(GetTickCount64()<deadline){LARGE_INTEGER q{};QueryPerformanceCounter(&q);reader.heartbeat(q.QuadPart);auto r=reader.read(b,planes.data());if(r==entropy::publish::ReadResult::Block){require(b.stream_epoch>epoch,"stale epoch");if(b.block_sequence>=3){for(unsigned c=0;c<channels;c++)for(double v:audio[c])if(std::abs(v-double(frame[c]))>=1e-6*std::abs(double(frame[c]))){std::cerr<<"DC c="<<c<<" actual="<<std::setprecision(17)<<v<<" expected="<<frame[c]<<" difference="<<v-double(frame[c])<<" block="<<b.block_sequence<<'\n';require(false,"channel/DC level");}valid=true;break;}}else require(r==entropy::publish::ReadResult::Empty,"reconnect gap");Sleep(1);}
  require(valid,"new stream");reader.close();output.stop();require(output.dropped()==0,"unexpected drops");std::cout<<"PASS transport "<<channels<<"ch valid silence, counter, channel order, generation and reconnect\n";
 }return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}


