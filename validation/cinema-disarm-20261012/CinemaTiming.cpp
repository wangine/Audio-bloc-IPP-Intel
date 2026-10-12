#include "StagePublishV2.h"
#include "StageLinkSecurity.h"
#include "Engine/HighResolutionWait.h"
#include <windows.h>
#include <avrt.h>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
struct Row { uint64_t kind,arrival,source,epoch,sequence,first,generation; };
int wmain(int argc,wchar_t**argv) {
 if(argc!=3)return 2;auto seconds=std::stoul(argv[1]);if(seconds>600)return 2;
 auto endpoint=entropy::link::endpointName();auto pos=endpoint.find(L"StageLink.V1");if(pos==std::wstring::npos)return 3;endpoint.replace(pos,12,L"StagePublish.Cinema.Decoder.V2");
 std::vector<Row> rows;rows.reserve(seconds*250+1000);
 std::array<std::array<double,1024>,8> audio{};std::array<double*,8> planes{};for(unsigned c=0;c<8;++c)planes[c]=audio[c].data();
 LARGE_INTEGER begin{},now{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);now=begin;
 DWORD task=0;auto mm=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);if(!mm||!AvSetMmThreadPriority(mm,AVRT_PRIORITY_CRITICAL))return 4;
 entropy::control::HighResolutionWait wait;if(!wait.valid())return 4;
 entropy::publish::Subscriber sub;bool connected=false;std::string error;unsigned faults=0;
 while(uint64_t(now.QuadPart-begin.QuadPart)<uint64_t(seconds)*frequency.QuadPart&&rows.size()<rows.capacity()){
  QueryPerformanceCounter(&now);
  if(!connected){connected=sub.connect(endpoint,error);QueryPerformanceCounter(&now);if(!connected){wait.forDuration(std::chrono::milliseconds(10));continue;}rows.push_back({1,uint64_t(now.QuadPart),0,0,0,0,0});}
  sub.heartbeat(now.QuadPart);bool received=false;
  for(unsigned batch=0;batch<32;++batch){StageBusBlockHeader h{};auto result=sub.read(h,planes.data());if(result==entropy::publish::ReadResult::Empty)break;
   QueryPerformanceCounter(&now);if(result!=entropy::publish::ReadResult::Block){rows.push_back({uint64_t(result)+10,uint64_t(now.QuadPart),0,0,0,0,0});++faults;sub.close();connected=false;break;}
   rows.push_back({0,uint64_t(now.QuadPart),h.qpc_timestamp,h.stream_epoch,h.block_sequence,h.first_frame,h.source_generation[2]});received=true;}
  if(!received)wait.forDuration(std::chrono::microseconds(100));
 }
 sub.close();AvRevertMmThreadCharacteristics(mm);
 std::ofstream out(argv[2]);out<<"kind,arrival_qpc,source_qpc,epoch,sequence,first_frame,generation\n";for(auto&r:rows)out<<r.kind<<','<<r.arrival<<','<<r.source<<','<<r.epoch<<','<<r.sequence<<','<<r.first<<','<<r.generation<<'\n';
 std::cout<<"qpcFrequency="<<frequency.QuadPart<<" rows="<<rows.size()<<" terminalReconnects="<<faults<<'\n';return 0;
}
