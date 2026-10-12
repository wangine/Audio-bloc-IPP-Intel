// Offline sample-grid verification of final ClockSync FP64 against captured Graph.
#include "Engine/Diagnostics/ChainAudit.h"
#include "Egress/ClockSync/include/EntropyBandlimitedAsrc.h"
#include "IPC/Json.h"
#include <iostream>
#include <cmath>
using namespace entropy;
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("GRAPH_OUTPUT_F64 CAPTURE_DIRECTORY");
 auto path=std::filesystem::u8path(argv[1]);HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("graph file");
 LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||size.QuadPart%(16*sizeof(double)))throw std::runtime_error("invalid graph frame file");HANDLE mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);auto* graph=static_cast<const double*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));if(!graph)throw std::runtime_error("graph mapping");
 const auto graphFrames=size.QuadPart/(16*sizeof(double));asrc::PolyphaseSinc::warmUp();std::cout<<"{\"sampleStride\":8,\"domains\":[";
 auto record=std::make_unique<chain_audit::SinkRecord>();auto dir=std::filesystem::u8path(argv[2]);
 bool anyFailed=false;
 for(unsigned domain=0;domain<2;++domain){if(domain)std::cout<<',';double previousPpm=0,max=0,square=0;uint64_t checked=0,failed=0,unavailable=0,partial=0,records=0;std::array<double,8> channelMax{};
  std::ifstream summaryFile(dir/(domain?"dac-b-summary.json":"dac-a-summary.json"));if(!summaryFile)throw std::runtime_error("missing DAC summary");auto summary=ipc::parse(std::string(std::istreambuf_iterator<char>(summaryFile),{}));auto expectedRecords=summary.at("written").asUnsigned();if(!expectedRecords||summary.at("recordBytes").asUnsigned()!=sizeof(*record)||summary.at("drops").asUnsigned()||summary.at("failed").asBool())throw std::runtime_error("incomplete DAC audit");
  for(unsigned part=0;;++part){std::ifstream in(dir/((domain?"dac-b-":"dac-a-")+std::to_string(part)+".bin"),std::ios::binary);if(!in)break;
   while(in.read(reinterpret_cast<char*>(record.get()),sizeof(*record))){auto& r=*record;++records;if(r.frames!=1024||r.channels!=8||!std::isfinite(r.firstGraph)||!std::isfinite(r.ppm))throw std::runtime_error("unsupported/invalid sink record");const double beginStep=1+previousPpm*1e-6,endStep=1+r.ppm*1e-6;previousPpm=r.ppm;if(!r.audio)continue;if(r.offset){++partial;continue;}
    double position=r.firstGraph;
    for(unsigned f=0;f<r.frames;++f){
     if(f%8==0){const auto lower=int64_t(std::floor(position));
      if(lower+asrc::PolyphaseSinc::kFirstOffset<0||lower+asrc::PolyphaseSinc::kLastOffset>=graphFrames)++unavailable;
      else {auto kernel=asrc::PolyphaseSinc::kernelFor(position-lower);
       for(unsigned ch=0;ch<r.channels;++ch){double expected=0;unsigned lane=domain*8+ch;
        for(int tap=0;tap<asrc::PolyphaseSinc::kTaps;++tap){double coefficient=kernel.first[tap]+(kernel.second[tap]-kernel.first[tap])*kernel.blend;expected+=coefficient*graph[(lower+asrc::PolyphaseSinc::kFirstOffset+tap)*16+lane];}
        if(!std::isfinite(expected)||!std::isfinite(r.rendered[ch][f]))throw std::runtime_error("nonfinite captured/reference DAC");double error=std::abs(expected-r.rendered[ch][f]);max=std::max(max,error);channelMax[ch]=std::max(channelMax[ch],error);square+=error*error;++checked;if(error>1e-8)++failed;
       }
      }
     }
     const double alpha=r.frames>1?double(f)/double(r.frames-1):1;position+=beginStep+(endStep-beginStep)*alpha;
    }
   }if(!in.eof()||in.gcount()!=0)throw std::runtime_error("truncated DAC record");
  }
  if(records!=expectedRecords||!checked)throw std::runtime_error("missing DAC records/parts or no audible checks");anyFailed=anyFailed||failed!=0;
  std::cout<<"{\"domain\":"<<domain<<",\"checkedSamples\":"<<checked<<",\"failedSamples\":"<<failed<<",\"maxError\":"<<max<<",\"rmsError\":"<<std::sqrt(square/std::max(uint64_t(1),checked))<<",\"unavailableEdgePositions\":"<<unavailable<<",\"startupPartialBlocks\":"<<partial<<",\"channelMaxError\":[";for(unsigned c=0;c<8;++c){if(c)std::cout<<',';std::cout<<channelMax[c];}std::cout<<"]}";
 }
 std::cout<<"]}\n";UnmapViewOfFile(graph);CloseHandle(mapping);CloseHandle(file);return anyFailed?3:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
