#include "StagePublishV2.h"
#include <windows.h>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
struct Record {uint64_t receiveQpc{}; StageBusBlockHeader header{};std::array<std::array<double,1024>,3> samples{};};
int main(int argc,char** argv){
 if(argc!=4){std::cerr<<"endpoint output seconds\n";return 1;}
 std::string name=argv[1];std::wstring endpoint(name.begin(),name.end());entropy::publish::Subscriber subscriber;std::string error;
 const unsigned seconds=std::stoul(argv[3]);if(!seconds||seconds>120)return 4;
 std::vector<Record> records((seconds+2)*192000/1024+8);size_t count=0;uint64_t empty=0;int terminal=0;
 if(!subscriber.connect(endpoint,error)){std::cerr<<error;return 2;}const auto session=subscriber.session();if(session.channels>3)return 3;
 LARGE_INTEGER f{},begin{},now{};QueryPerformanceFrequency(&f);QueryPerformanceCounter(&begin);uint64_t lastBeat=0;
 for(;;){QueryPerformanceCounter(&now);if(now.QuadPart-begin.QuadPart>=int64_t(seconds)*f.QuadPart)break;
  if(count==records.size()){terminal=99;break;}auto& record=records[count];double* planes[]{record.samples[0].data(),record.samples[1].data(),record.samples[2].data()};
  if(uint64_t(now.QuadPart)-lastBeat>uint64_t(f.QuadPart)/10){subscriber.heartbeat(now.QuadPart);lastBeat=now.QuadPart;}
  const auto result=subscriber.read(record.header,planes);
  if(result==entropy::publish::ReadResult::Block){record.receiveQpc=now.QuadPart;++count;}
  else if(result==entropy::publish::ReadResult::Empty){++empty;Sleep(1);}
  else {terminal=int(result);break;}
 }
 subscriber.close(); // No filesystem I/O while observing publication.
 std::ofstream file(std::filesystem::u8path(argv[2]),std::ios::binary);file.write(reinterpret_cast<const char*>(&session),sizeof(session));
 for(size_t i=0;i<count;++i)file.write(reinterpret_cast<const char*>(&records[i]),sizeof(Record));file.close();
 std::cout<<"{\"records\":"<<count<<",\"channels\":"<<session.channels<<",\"recordBytes\":"<<sizeof(Record)<<",\"emptyPolls\":"<<empty<<",\"terminal\":"<<terminal<<",\"qpcFrequency\":"<<f.QuadPart<<"}\n";return terminal||!file?5:0;
}
