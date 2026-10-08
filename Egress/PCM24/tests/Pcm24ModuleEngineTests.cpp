#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
namespace{
EntropyPcm24Config cfg(unsigned channels=1){EntropyPcm24Config c{};c.struct_size=sizeof(c);c.dither_mode=ENTROPY_PCM24_DITHER_NONE;c.seed_mode=ENTROPY_PCM24_SEED_DETERMINISTIC_TEST;c.channels=channels;c.seed=0x5450444654455354ULL;c.output_format=ENTROPY_PCM24_PACKED_LE;c.maximum_frames=2048;c.revision=5;return c;}
bool legacy(){entropy::pcm24::Pcm24ModuleEngine e;if(!e.configure(cfg()))return false;std::array<double,3>in{-1,0,1};const double*ip=in.data();std::array<std::uint8_t,9>out{};EntropyPcm24BlockF64 b{};b.struct_size=sizeof(b);b.channels=1;b.frames=3;b.inputs=&ip;b.output_interleaved=out.data();b.output_capacity_bytes=out.size();const std::array<std::uint8_t,9>expected{0,0,0x80,0,0,0,0xff,0xff,0x7f};bool ok=e.process(b)==ENTROPY_OK&&b.produced_bytes==9&&out==expected;auto c=cfg();c.output_format=ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED;ok=ok&&e.configure(c);std::array<std::uint8_t,12>q{};b.output_interleaved=q.data();b.output_capacity_bytes=q.size();ok=ok&&e.process(b)==ENTROPY_OK&&q==std::array<std::uint8_t,12>{0,0,0,0x80,0,0,0,0,0,0xff,0xff,0x7f};std::printf("legacy packed + 24-in-32 %s\n",ok?"PASS":"FAIL");return ok;}
bool deterministic(){entropy::pcm24::Pcm24ModuleEngine e;auto c=cfg(2);c.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;if(!e.configure(c))return false;std::array<std::vector<double>,2>in{std::vector<double>(1024),std::vector<double>(1024)};std::array<const double*,2>ip{in[0].data(),in[1].data()};std::vector<std::uint8_t>a(1024*2*3),b(a.size());EntropyPcm24BlockF64 x{};x.struct_size=sizeof(x);x.channels=2;x.frames=1024;x.inputs=ip.data();x.output_capacity_bytes=a.size();x.output_interleaved=a.data();if(e.process(x)!=ENTROPY_OK)return false;e.reset();x.output_interleaved=b.data();if(e.process(x)!=ENTROPY_OK)return false;bool nonzero=false,decorrelated=false;for(std::size_t i=0;i<a.size();++i)nonzero=nonzero||a[i]!=0;for(std::size_t f=0;f<1024;++f)decorrelated=decorrelated||std::memcmp(a.data()+f*6,a.data()+f*6+3,3)!=0;bool ok=a==b&&nonzero&&decorrelated;std::printf("deterministic TPDF replay/decorrelation %s\n",ok?"PASS":"FAIL");return ok;}
bool productionAndFault(){entropy::pcm24::Pcm24ModuleEngine e;auto c=cfg();c.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;c.seed_mode=ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT;if(!e.configure(c))return false;std::vector<double>in(128);const double*ip=in.data();std::vector<std::uint8_t>a(384),b(384);EntropyPcm24BlockF64 x{};x.struct_size=sizeof(x);x.channels=1;x.frames=128;x.inputs=&ip;x.output_capacity_bytes=a.size();x.output_interleaved=a.data();if(e.process(x)!=ENTROPY_OK)return false;e.reset();x.output_interleaved=b.data();if(e.process(x)!=ENTROPY_OK)return false;bool continued=a!=b;in[7]=std::numeric_limits<double>::quiet_NaN();std::fill(b.begin(),b.end(),0xff);auto bad=e.process(x);std::fill(in.begin(),in.end(),0);std::fill(b.begin(),b.end(),0xff);auto held=e.process(x);auto t=e.telemetry();bool zero=true;for(auto v:b)zero=zero&&v==0;bool ok=continued&&bad==ENTROPY_ERROR_PROCESSING&&held==ENTROPY_ERROR_PROCESSING&&zero&&(t.fault_flags&ENTROPY_PCM24_FAULT_NONFINITE_AUDIO)&&t.nonfinite_sample_count==1;std::printf("production continuity + nonfinite latch %s\n",ok?"PASS":"FAIL");return ok;}}
bool clipContinues(){
 entropy::pcm24::Pcm24ModuleEngine e;auto c=cfg();c.output_format=ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED;c.reserved_u32=1;
 if(!e.configure(c))return false;
 std::array<double,3> input{1.0266780766718209,-1.0266780766718209,0.75/8388608.0};const double* ip=input.data();std::array<uint8_t,12> out{};
 EntropyPcm24BlockF64 b{};b.struct_size=sizeof(b);b.channels=1;b.frames=3;b.inputs=&ip;b.output_interleaved=out.data();b.output_capacity_bytes=out.size();
 bool ok=e.process(b)==ENTROPY_OK&&b.produced_bytes==12&&out==std::array<uint8_t,12>{0,255,255,127,0,0,0,128,0,1,0,0};auto t=e.telemetry();ok=ok&&t.fault_flags==0&&t.overrange_sample_count==2&&t.reserved[1]==0&&t.reserved[2]==1;
 input.fill(0);ok=ok&&e.process(b)==ENTROPY_OK;input[0]=std::numeric_limits<double>::quiet_NaN();ok=ok&&e.process(b)==ENTROPY_ERROR_PROCESSING;input.fill(0);ok=ok&&e.process(b)==ENTROPY_ERROR_PROCESSING;
 std::printf("declared CLIP continues, exact PCM24 rails, nonfinite remains fatal %s\n",ok?"PASS":"FAIL");return ok;
}
int main(){bool ok=clipContinues();ok=legacy()&&ok;ok=deterministic()&&ok;ok=productionAndFault()&&ok;return ok?0:1;}

