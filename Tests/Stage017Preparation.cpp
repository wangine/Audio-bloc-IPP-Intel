#include "Engine/Control/ControlPlane.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Control/LiveEligibility.h"
#include <cmath>
#include <cstring>
#include <iostream>
using namespace entropy;
using J=ipc::Value;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main()try{
 auto cached=std::make_unique<engine::AudioGraph>(),fresh=std::make_unique<engine::AudioGraph>();
 check(cached->prepare(64)&&fresh->prepare(64),"prepare");control::ControlPlane a(*cached),b(*fresh);
 a.execute(J::Object{{"action","parameters"},{"values",J::Array{
  J::Object{{"id","processMode1"},{"value",1}},J::Object{{"id","firLength1"},{"value",0}},
  J::Object{{"id","lpOn1"},{"value",1}},J::Object{{"id","lpHz1"},{"value",12000}},
  J::Object{{"id","musicDelayMs1"},{"value",.125}},J::Object{{"id","fractionalDelayMode1"},{"value",2}}}}});
 // The original native scene restore can snap initial default floats on its
 // first roundtrip (e.g. 20000.001953 -> 20000). That is a real effective DSP
 // change and must rebuild; no tolerance or key rounding is permitted here.
 a.execute(J::Object{{"action","program"},{"program",1}});a.execute(J::Object{{"action","program"},{"program",0}});
 a.execute(J::Object{{"action","program"},{"program",1}});a.execute(J::Object{{"action","program"},{"program",0}});
 check(a.crossover().preparationCache().at("hits").asU64()>0,"return to prepared MUSIC recompiles kernels");
 auto builds=a.crossover().preparationCache().at("builds").asU64();
 a.execute(J::Object{{"action","parameter"},{"id","selectedChannel"},{"value",7}});
 a.execute(J::Object{{"action","monitor"},{"channel",3}});
 a.execute(J::Object{{"action","gesture"},{"id","hpHz1"},{"begin",true}});
 a.execute(J::Object{{"action","gesture"},{"id","hpHz1"},{"begin",false}});
 check(a.crossover().preparationCache().at("builds").asU64()==builds,"UI/FFT/gesture invalidated preparation");
 a.execute(J::Object{{"action","parameter"},{"id","musicDelayMs1"},{"value",.25}});++builds;
 check(a.crossover().preparationCache().at("builds").asU64()==builds,"effective delay did not invalidate MUSIC");
 a.execute(J::Object{{"action","program"},{"program",1}});
 check(a.crossover().preparationCache().at("builds").asU64()==builds,"MUSIC-only change invalidated CINEMA");
 a.execute(J::Object{{"action","program"},{"program",0}});
 b.crossover().importBytes(a.crossover().exportBytes());
 a.crossover().resetQuiescent();b.crossover().resetQuiescent();
 check(a.crossover().preparedFinite()&&b.crossover().preparedFinite(),"nonfinite coefficients");
 auto x=engine::GraphControlAccess::crossover(*cached),y=engine::GraphControlAccess::crossover(*fresh);
 entropy::native::AudioBuffer<double> left(16,64),right(16,64);entropy::native::MidiBuffer midi;uint64_t active=0;
 for(unsigned block=0;block<300;++block){
  for(int ch=0;ch<16;++ch)for(int n=0;n<64;++n){const double v=ch<11?.003*std::sin((block*64+n)*.013*(ch+1)):0;left.getWritePointer(ch)[n]=right.getWritePointer(ch)[n]=v;}
  a.crossover().captureBeforeBlock();x->processBlock(left,midi);a.crossover().captureAfterBlock();
  b.crossover().captureBeforeBlock();y->processBlock(right,midi);b.crossover().captureAfterBlock();
  for(int ch=0;ch<16;++ch){check(std::memcmp(left.getReadPointer(ch),right.getReadPointer(ch),64*sizeof(double))==0,"cached coefficients changed FP64 output");for(int n=0;n<64;++n)active+=left.getReadPointer(ch)[n]!=0;}
 }
 check(active>10000,"all-zero numeric false PASS");
 // Saved target scene limits are validated before publishing a live switch.
 a.execute(J::Object{{"action","program"},{"program",1}});J::Array large;
 for(int ch=1;ch<=16;++ch){large.emplace_back(J::Object{{"id","processMode"+std::to_string(ch)},{"value",1}});large.emplace_back(J::Object{{"id","firLength"+std::to_string(ch)},{"value",2}});}
 a.execute(J::Object{{"action","parameters"},{"values",large}});a.execute(J::Object{{"action","program"},{"program",0}});
 check(control::liveFirAllowed(a.crossover().preparationParameters()),"source FIR profile wrong");
 check(!control::liveFirAllowed(a.crossover().preparationParameters(J::Object{{"action","program"},{"program",1}})),"saved FIR8193x16 bypasses scene validation");
 std::cout<<ipc::dump(J::Object{{"PASS",true},{"comparedSamples",300*64*16},{"maximumAbsoluteDifference",0},{"bitExact",true},{"nonzeroSamples",J::u64(active)},{"preparedCache",a.crossover().preparationCache()}})<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
