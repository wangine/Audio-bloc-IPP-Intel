#include "CinemaOutput.h"
#include "StagePublishV2.h"
#include "CinemaDeadlineWait.h"
#include <windows.h>
#include <array>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <iostream>
int main(){CinemaOutput output;std::string error;auto endpoint=CinemaOutput::defaultEndpoint()+L".Cadence."+std::to_wstring(GetCurrentProcessId());if(!output.start(6,endpoint,error)){std::cerr<<error;return 2;}
 entropy::publish::Subscriber reader;if(!reader.connect(endpoint,error)){std::cerr<<error;return 2;}
 std::atomic<bool> done=false;std::atomic<uint64_t> maxFeedGap=0;
 std::thread producer([&]{CinemaDeadlineWait wait;std::array<float,8> f{};f.fill(.125f);auto next=std::chrono::steady_clock::now();uint64_t previous=0;for(unsigned b=0;b<3750;++b){LARGE_INTEGER q{};QueryPerformanceCounter(&q);if(previous)maxFeedGap.store((std::max)(maxFeedGap.load(),uint64_t(q.QuadPart)-previous));previous=q.QuadPart;for(unsigned i=0;i<128;++i)output.submit(f);next+=std::chrono::nanoseconds(2666667);wait.until(next);}done=true;});
 std::array<std::array<double,1024>,8> audio{};std::array<double*,8> planes{};for(unsigned c=0;c<8;++c)planes[c]=audio[c].data();
 std::vector<double> gaps;gaps.reserve(2000);uint64_t previous=0,sequence=0;bool bad=false;auto deadline=GetTickCount64()+12000;unsigned blocks=0;CinemaDeadlineWait wait;
 while(GetTickCount64()<deadline){LARGE_INTEGER q{};QueryPerformanceCounter(&q);reader.heartbeat(q.QuadPart);StageBusBlockHeader h{};auto r=reader.read(h,planes.data());if(r==entropy::publish::ReadResult::Empty){if(done&&blocks>=1873)break;wait.forDuration(std::chrono::microseconds(100));continue;}if(r!=entropy::publish::ReadResult::Block){bad=true;break;}if(sequence&&h.block_sequence!=sequence+1)bad=true;sequence=h.block_sequence;if(previous)gaps.push_back(double(h.qpc_timestamp-previous)/10000.);previous=h.qpc_timestamp;if(h.block_sequence>3)for(unsigned c=0;c<6;++c)for(auto v:audio[c])if(std::abs(v-.125)>1e-6)bad=true;++blocks;}
 producer.join();reader.close();output.stop();std::sort(gaps.begin(),gaps.end());if(gaps.empty())return 2;auto p99=gaps[size_t(.99*(gaps.size()-1))];std::cout<<"blocks="<<blocks<<" sourceGapP99Ms="<<p99<<" sourceGapMaxMs="<<gaps.back()<<" feedGapMaxMs="<<maxFeedGap.load()/10000.<<" drops="<<output.dropped()<<" PCM/counterErrors="<<bad<<'\n';bool pass=blocks>=1873&&!bad&&!output.dropped()&&p99<12.;std::cout<<(pass?"PASS":"FAIL")<<" paced committed PCM publication cadence\n";return pass?0:1;}

