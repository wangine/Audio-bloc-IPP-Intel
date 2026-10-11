#include "Engine/Input/InputContract.h"
#include "Engine/HighResolutionWait.h"
#include "StagePublishV2.h"
#include <windows.h>
#include <avrt.h>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
struct Row { unsigned stage; uint64_t sequence,arrival,source,generation; double peak; };
int wmain(int argc,wchar_t** argv) {
 if(argc!=3)return 2;
 unsigned seconds=std::stoul(argv[1]);if(seconds>600)return 2;
 std::array<entropy::publish::Subscriber,2> in;
 auto dsp=entropy::input::defaultEndpoint(entropy::input::Domain::Music);
 auto source=dsp;source.replace(source.find(L".Dsp."),5,L".Source.");
 std::string error;if(!in[0].connect(source,error)||!in[1].connect(dsp,error)){std::cerr<<error;return 3;}
 std::array<std::array<double,1024>,8> audio{};std::array<double*,8> planes{};for(unsigned c=0;c<8;++c)planes[c]=audio[c].data();
 std::vector<Row> rows;rows.reserve(seconds*400+1000);
 LARGE_INTEGER frequency{},begin{},now{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
 DWORD task=0;auto mmcss=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);if(!mmcss||!AvSetMmThreadPriority(mmcss,AVRT_PRIORITY_CRITICAL))return 4;
 entropy::control::HighResolutionWait wait;if(!wait.valid())return 4;
 unsigned faults=0;std::cout<<"Connected both read-only observers\n"<<std::flush;
 do {bool received=false;QueryPerformanceCounter(&now);
  for(unsigned stage=0;stage<2;++stage){in[stage].heartbeat(now.QuadPart);
   for(unsigned batch=0;batch<16;++batch){StageBusBlockHeader h{};auto r=in[stage].read(h,planes.data());if(r==entropy::publish::ReadResult::Empty)break;if(r!=entropy::publish::ReadResult::Block){++faults;break;}
    QueryPerformanceCounter(&now);double peak=0;for(unsigned c=0;c<in[stage].session().channels;++c)for(double x:audio[c])peak=(std::max)(peak,std::abs(x));
    rows.push_back({stage+1,h.block_sequence,uint64_t(now.QuadPart),h.qpc_timestamp,h.source_generation[0],peak});received=true;}
  }
  if(!received)wait.forDuration(std::chrono::microseconds(100));
 }while(!faults&&uint64_t(now.QuadPart-begin.QuadPart)<uint64_t(seconds)*frequency.QuadPart&&rows.size()<rows.capacity());
 AvRevertMmThreadCharacteristics(mmcss);
 std::ofstream out(argv[2]);out<<"stage,sequence,arrival_qpc,source_qpc,generation,peak\n";out.precision(17);for(const auto& r:rows)out<<r.stage<<','<<r.sequence<<','<<r.arrival<<','<<r.source<<','<<r.generation<<','<<r.peak<<'\n';
 std::cout<<"rows="<<rows.size()<<" qpcFrequency="<<frequency.QuadPart<<" faults="<<faults<<'\n';return faults?1:0;
}
