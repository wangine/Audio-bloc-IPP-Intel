#include "CinemaOutput.h"
#include "StagePublishV2.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
#include <stdexcept>
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
struct Capture {std::vector<double> samples;unsigned channels=8;static void block(void* ctx,const double* const* p){auto& c=*static_cast<Capture*>(ctx);for(unsigned i=0;i<1024;i++)for(unsigned ch=0;ch<c.channels;ch++)c.samples.push_back(p[ch][i]);}};
int main(){try{
 for(unsigned channels:{6u,8u}){
  CinemaBlockProcessor processor;require(processor.prepare(channels),"prepare");Capture c;c.channels=channels;
  std::array<std::vector<double>,8> input,reference;std::array<const double*,8> ip{};std::array<double*,8> op{};
  entropy::host::CinemaWdlSrc offline;require(offline.prepare(1024,128)&&offline.configure(48000),"reference");
  const unsigned capacity=offline.outputCapacity();for(unsigned ch=0;ch<8;ch++){input[ch].resize(1024);reference[ch].resize(capacity);ip[ch]=input[ch].data();op[ch]=reference[ch].data();}
  std::vector<double> expected;
  for(unsigned chunk=0;chunk<48;chunk++){
   for(unsigned i=0;i<1024;i++){std::array<float,8> f{};for(unsigned ch=0;ch<channels;ch++){f[ch]=float(.05*(ch+1)*std::sin(6.283185307179586*(997+ch*240)*(chunk*1024+i)/48000.));input[ch][i]=double(f[ch]);}for(unsigned ch=channels;ch<8;ch++)input[ch][i]=0;require(processor.push(f,Capture::block,&c),"processing");}
   unsigned made=0;require(offline.process(ip.data(),1024,op.data(),capacity,made),"reference process");for(unsigned i=0;i<made;i++)for(unsigned ch=0;ch<channels;ch++)expected.push_back(reference[ch][i]);
  }
  require(c.samples.size()>channels*190000,"output count");double worst=0;for(size_t i=0;i<c.samples.size();i++)worst=std::max(worst,std::abs(c.samples[i]-expected.at(i)));require(worst<1e-12,"segmentation/reference mismatch");
  processor.reset();Capture silence;silence.channels=channels;for(int i=0;i<4096;i++)require(processor.push({},Capture::block,&silence),"silence");require(!silence.samples.empty(),"valid silence missing");for(double v:silence.samples)require(v==0,"history retained after reset");
  std::cout<<"PASS channels="<<channels<<" reference max_error="<<worst<<" output_frames="<<c.samples.size()/channels<<" valid silence/reset\n";
 }
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
