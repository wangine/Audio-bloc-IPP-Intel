#include "Engine/Input/InputContract.h"
#include "StagePublishV2.h"
#include <windows.h>
#include <chrono>
#include <array>
#include <cmath>
#include <iostream>
int wmain(int argc,wchar_t** argv){unsigned seconds=argc>2?std::stoul(argv[2]):10;auto endpoint=argc>1?std::wstring(argv[1]):entropy::input::defaultEndpoint(entropy::input::Domain::Music);entropy::publish::Subscriber s;std::string error;if(!s.connect(endpoint,error)){std::cerr<<error;return 2;}const auto session=s.session();if(!entropy::input::validateSessionForDomain(session,entropy::input::Domain::Music))return 3;
 std::array<std::array<double,1024>,3> audio{};std::array<double*,3> p{audio[0].data(),audio[1].data(),audio[2].data()};std::array<double,3> peaks{};uint64_t blocks=0,first=0,last=0,nonfinite=0,failures=0,valid=0;StageBusBlockHeader h{};auto start=std::chrono::steady_clock::now();
 while(std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)){LARGE_INTEGER q{};QueryPerformanceCounter(&q);s.heartbeat(q.QuadPart);auto r=s.read(h,p.data());if(r==entropy::publish::ReadResult::Empty){Sleep(1);continue;}if(r!=entropy::publish::ReadResult::Block||!entropy::input::validateBlockForDomain(session,h,entropy::input::Domain::Music)){++failures;break;}if(!blocks)first=h.block_sequence;last=h.block_sequence;++blocks;if(h.validity_mask&1)++valid;for(unsigned c=0;c<3;++c)for(auto v:audio[c]){if(!std::isfinite(v))++nonfinite;else peaks[c]=(std::max)(peaks[c],std::abs(v));}}
 std::string uuid;const char hex[]="0123456789abcdef";for(auto b:session.session_id){uuid+=hex[b>>4];uuid+=hex[b&15];}
 std::cout<<"{\"channels\":3,\"rate\":192000,\"layout\":4,\"blocks\":"<<blocks<<",\"validMusicBlocks\":"<<valid<<",\"firstSequence\":"<<first<<",\"lastSequence\":"<<last<<",\"transportFailures\":"<<failures<<",\"nonfiniteSamples\":"<<nonfinite<<",\"peakL\":"<<peaks[0]<<",\"peakR\":"<<peaks[1]<<",\"peakC\":"<<peaks[2]<<",\"sourceSession\":\""<<uuid<<"\"}\n";return blocks>=1000&&valid>=1000&&!failures&&!nonfinite&&last-first+1==blocks?0:1;
}
