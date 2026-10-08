#include "HeadroomEngine.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <new>
namespace{std::atomic<unsigned long long>a{0};}void*operator new(std::size_t n){a.fetch_add(1);if(auto*p=std::malloc(n))return p;throw std::bad_alloc{};}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){entropy::headroom::HeadroomEngine e;if(!e.prepare(192000,256))return 1;EntropyHeadroomState s{};s.struct_size=sizeof(s);s.revision=1;s.group_count=1;s.active_channel_mask=0xffff;s.transition_ms=5;for(auto&v:s.channel_group)v=0;s.groups[0].struct_size=sizeof(s.groups[0]);s.groups[0].bounded=1;s.groups[0].declared_positive_contribution_db=3;if(e.setState(s)!=ENTROPY_OK||e.clearFault()!=ENTROPY_OK)return 2;std::array<std::array<double,256>,16>in{},out{};std::array<const double*,16>ip{};std::array<double*,16>op{};for(unsigned c=0;c<16;++c){ip[c]=in[c].data();op[c]=out[c].data();}EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.input_channels=16;b.output_channels=16;b.frames=256;b.inputs=ip.data();b.outputs=op.data();
EntropyHeadroomAnalysisBlockF64 analysis{};analysis.struct_size=sizeof(analysis);analysis.channels=16;analysis.frames=256;analysis.inputs=ip.data();
auto before=a.load();for(int i=0;i<4096;++i){in[0].fill(i<1024?2.0:0.125);if(e.analyzeFinal(analysis)!=ENTROPY_OK||e.process(b)!=ENTROPY_OK)return 3;const auto telemetry=e.telemetry();if(telemetry.fault_flags)return 5;}return a.load()==before?0:4;}
