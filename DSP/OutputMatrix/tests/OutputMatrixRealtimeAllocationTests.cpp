#include "OutputMatrixEngine.h"
#include <array>
#include <atomic>
#include <cstdlib>
#include <new>
namespace{std::atomic<unsigned long long>a{0};}void*operator new(std::size_t n){a.fetch_add(1);if(auto*p=std::malloc(n))return p;throw std::bad_alloc{};}void*operator new[](std::size_t n){return::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){entropy::output_matrix::OutputMatrixEngine e;if(!e.prepare(192000,256))return 1;EntropyOutputMatrixState s{};s.struct_size=sizeof(s);s.active_source_mask=0xffff;s.enabled_destination_mask=0xffff;for(unsigned i=0;i<16;++i)s.source_destination_mask[i]=1u<<i;if(e.setState(s)!=ENTROPY_OK)return 2;std::array<std::array<double,256>,16>in{},out{};std::array<const double*,16>ip{};std::array<double*,16>op{};for(unsigned i=0;i<16;++i){ip[i]=in[i].data();op[i]=out[i].data();}EntropyAudioBlockF64 b{};b.struct_size=sizeof(b);b.input_channels=16;b.output_channels=16;b.frames=256;b.inputs=ip.data();b.outputs=op.data();auto before=a.load();for(int i=0;i<512;++i)if(e.process(b)!=ENTROPY_OK)return 3;return a.load()==before?0:4;}
