#include "CinemaOutput.h"
#include "StagePublishV2.h"
#include <windows.h>
#include <array>
#include <iostream>
#include <stdexcept>
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(){try{auto endpoint=CinemaOutput::defaultEndpoint()+L".Isolation."+std::to_wstring(GetCurrentProcessId());CinemaOutput output;std::string error;require(output.start(6,endpoint,error),"start");
 for(int batch=0;batch<12;batch++){for(int i=0;i<4096;i++)output.submit({});Sleep(15);}Sleep(30);require(output.blocks()>180&&output.dropped()==0,"no consumer obstructed source");
 entropy::publish::Subscriber slow;require(slow.connect(endpoint,error),"slow connect");for(int batch=0;batch<12;batch++){for(int i=0;i<4096;i++)output.submit({});Sleep(15);}Sleep(30);require(output.dropped()>0,"slow subscriber loss not explicit");
 std::array<std::array<double,1024>,8> samples{};std::array<double*,8> planes{};for(unsigned c=0;c<8;c++)planes[c]=samples[c].data();StageBusBlockHeader b{};require(slow.read(b,planes.data())==entropy::publish::ReadResult::Gap,"slow reader not fail closed");slow.close();output.invalidate();Sleep(30);require(slow.connect(endpoint,error),"idle connect");const auto blocks=output.blocks();Sleep(100);require(output.blocks()==blocks,"manufactured idle PCM");require(slow.read(b,planes.data())==entropy::publish::ReadResult::Empty,"idle false PCM");slow.close();output.stop();std::cout<<"PASS absent subscriber, slow reader explicit Gap, idle emits no PCM\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
