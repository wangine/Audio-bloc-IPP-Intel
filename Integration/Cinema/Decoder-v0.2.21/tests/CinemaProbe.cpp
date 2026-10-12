#include "StagePublishV2.h"
#include "StageLinkSecurity.h"
#include "StageBusValidation.h"
#include <windows.h>
#include <array>
#include <iostream>
int wmain(int argc,wchar_t** argv){
 auto endpoint=entropy::link::endpointName();const auto at=endpoint.find(L"StageLink.V1");if(at==std::wstring::npos)return 2;
 endpoint.replace(at,12,L"StagePublish.Cinema.Decoder.V2");if(argc>1)endpoint=argv[1];
 entropy::publish::Subscriber reader;std::string error;if(!reader.connect(endpoint,error)){std::cerr<<"FAIL CINEMA FP64/192 endpoint: "<<error<<"\n";return 1;}
 auto s=reader.session();if(entropy::stage::validateSession(s,s.mapping_bytes)!=entropy::stage::StageBusValidationResult::Ok||!(s.channels==6||s.channels==8)){std::cerr<<"FAIL CINEMA contract\n";return 1;}
 std::array<std::array<double,1024>,8> audio{};std::array<double*,8> p{};for(unsigned c=0;c<8;c++)p[c]=audio[c].data();unsigned count=0;const unsigned target=argc>2?std::max(188u,unsigned(wcstoul(argv[2],nullptr,10))):188u;auto end=GetTickCount64()+std::max(10000ull,static_cast<unsigned long long>(target)*6+5000);
 std::array<double,8> peaks{};while(GetTickCount64()<end&&count<target){LARGE_INTEGER q{};QueryPerformanceCounter(&q);reader.heartbeat(q.QuadPart);StageBusBlockHeader b{};auto r=reader.read(b,p.data());if(r==entropy::publish::ReadResult::Block){if(b.validity_mask!=4||b.source_rate[2]!=48000)return 1;++count;for(unsigned c=0;c<s.channels;c++)for(double v:audio[c])peaks[c]=std::max(peaks[c],std::abs(v));continue;}else if(r!=entropy::publish::ReadResult::Empty){std::cerr<<"FAIL read="<<int(r)<<" after "<<count<<" blocks\n";return 1;}Sleep(1);}
 if(count<target){std::cerr<<"FAIL insufficient valid PCM: "<<count<<"\n";return 1;}std::cout<<"PASS Cinema channels="<<s.channels<<" rate="<<s.sample_rate<<" blocks="<<count<<" frames="<<count*1024<<" peaks=";for(unsigned c=0;c<s.channels;c++)std::cout<<peaks[c]<<',';std::cout<<'\n';return 0;
}


