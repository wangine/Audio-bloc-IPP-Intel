#include "TestSupport.h"
#include "TransparentDspExecutor.h"
int main(){
 auto src=session(1),dst=makeSession(2,src.session_id,src.qpc_frequency);
 Planes<10> input;struct Guarded{uint64_t before=0xdeadbeef;Planes<11> output;uint64_t after=0x12345678;} guarded;
 TransparentDspExecutor executor;
 uint64_t noise=0xabcdef192;
 for(uint64_t seq=1;seq<=12;seq++){
  for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++){
   noise^=noise<<13;noise^=noise>>7;noise^=noise<<17;
   uint64_t v=noise;
   if(i==0)v=0; if(i==1)v=0x8000000000000000ULL;if(i==2)v=1;if(i==3)v=0x0010000000000000ULL;
   if(i==1023)v=0x4014000000000000ULL+c;
   std::memcpy(&input.data[c][i],&v,8);
  }
  std::memset(guarded.output.data.data(),0xff,sizeof(guarded.output.data));
  auto inHeader=header(request(src,seq));StageBusBlockHeader outHeader{};
  CHECK(executor.process(src,dst,inHeader,input.in.data(),outHeader,guarded.output.out.data())==DspExecuteResult::Ok);
  CHECK(!std::memcmp(&inHeader,&outHeader,128));
  for(unsigned c=0;c<10;c++)CHECK(!std::memcmp(input.in[c],guarded.output.in[c<2?c:c+1],8192));
  for(auto x:guarded.output.data[2])CHECK(bits(x)==0);
  CHECK(guarded.before==0xdeadbeef && guarded.after==0x12345678);
 }
 auto b=header(request(src,1));auto old=guarded.output.data;
 auto bad=src;bad.sample_rate=96000;
 CHECK(executor.process(bad,dst,b,input.in.data(),b,guarded.output.out.data())==DspExecuteResult::Invalid);
 bad=dst;bad.layout_id=1;CHECK(executor.process(src,bad,b,input.in.data(),b,guarded.output.out.data())==DspExecuteResult::Invalid);
 bad=dst;bad.session_id[0]^=1;CHECK(executor.process(src,bad,b,input.in.data(),b,guarded.output.out.data())==DspExecuteResult::Invalid);
 CHECK(executor.process(src,dst,b,nullptr,b,guarded.output.out.data())==DspExecuteResult::Invalid);
 auto invalid=b;invalid.frames=100;CHECK(executor.process(src,dst,invalid,input.in.data(),b,guarded.output.out.data())==DspExecuteResult::Invalid);
 CHECK(!std::memcmp(old.data(),guarded.output.data.data(),sizeof(old)));
 std::cout<<"PASS 122880 FP64 bit patterns, 11-slot mapping, CX positive zero, header identity, canaries and invalid input rejection\n";
}
