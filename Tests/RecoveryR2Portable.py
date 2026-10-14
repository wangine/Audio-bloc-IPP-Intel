"""Focused R2 checks, not Windows/MSVC, real StagePublish IPC or DAC qualification.

Compiles the actual concurrency header, then the complete DomainIngress.cpp and
ProgramIngress.cpp method bodies with deterministic Windows/transport substitutes.
Only #include directives are replaced in the temporary adapter translation unit.
The substitutes do NOT validate binary ABI layout or OS/driver scheduling.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUBS = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include "Engine/Input/IngressConcurrency.h"
using DWORD=unsigned;using HANDLE=void*;
struct LARGE_INTEGER {int64_t QuadPart=0;};
void QueryPerformanceCounter(LARGE_INTEGER* q){q->QuadPart=std::chrono::steady_clock::now().time_since_epoch().count();}
uint64_t GetTickCount64(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void Sleep(unsigned n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
HANDLE AvSetMmThreadCharacteristicsW(const wchar_t*,DWORD*){return reinterpret_cast<HANDLE>(1);}
bool AvSetMmThreadPriority(HANDLE,int){return true;}void AvRevertMmThreadCharacteristics(HANDLE){}
constexpr int AVRT_PRIORITY_CRITICAL=1;constexpr bool FALSE=false;constexpr unsigned ERROR_ALREADY_EXISTS=183;
HANDLE CreateMutexW(void*,bool,const wchar_t*){return reinterpret_cast<HANDLE>(1);}unsigned GetLastError(){return 0;}void CloseHandle(HANDLE){}
struct StageBusSessionHeader {uint8_t session_id[16]{};unsigned channels=3,producer_pid=42;};
struct StageBusBlockHeader {
 uint64_t qpc_timestamp=0,source_generation[3]{},stream_epoch=1,block_sequence=0,first_frame=0,music_session_revision=1;
 unsigned validity_mask=1,discontinuity_mask=0,frames=1024;
};
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
template<class F>void until(F f,const char* s){auto end=GetTickCount64()+2000;while(!f()){if(GetTickCount64()>end)throw std::runtime_error(s);std::this_thread::sleep_for(std::chrono::microseconds(50));}}
double sample(uint64_t n,unsigned c,unsigned f){if(f==0)return -0.0;if(f==1)return std::bit_cast<double>(uint64_t{1});return double(n*8192+c*1024+f)/1048576.0;}
struct FakeBus {std::mutex mutex;std::deque<uint64_t> blocks;StageBusSessionHeader session;std::atomic<bool> allowConnect{true};std::atomic<unsigned> connects{0},closes{0};};
FakeBus buses[2];
void setupBus(unsigned channels){for(auto& b:buses){std::lock_guard g(b.mutex);b.blocks.clear();b.session={};b.session.channels=channels;b.session.session_id[0]=1;b.allowConnect=true;b.connects=0;b.closes=0;}}
void send(uint64_t n){std::lock_guard g(buses[0].mutex);buses[0].blocks.push_back(n);}
namespace entropy::control {struct HighResolutionWait {bool valid(){return true;}template<class T>void forDuration(T t){std::this_thread::sleep_for(t);}};}
namespace entropy::publish {
enum class ReadResult {Empty,Block,Invalid,Gap,Disconnected};
class Subscriber {
 FakeBus* bus_=nullptr;StageBusSessionHeader s_{};
public:
 bool connect(const std::wstring& name,std::string&){bus_=&buses[name==L"cinema"?1:0];if(!bus_->allowConnect)return false;std::lock_guard g(bus_->mutex);s_=bus_->session;++bus_->connects;return true;}
 void close(){if(bus_)++bus_->closes;bus_=nullptr;}
 void heartbeat(uint64_t){}const StageBusSessionHeader& session(){return s_;}
 ReadResult read(StageBusBlockHeader& h,double* const* planes){
  if(!bus_)return ReadResult::Disconnected;std::lock_guard g(bus_->mutex);if(bus_->blocks.empty())return ReadResult::Empty;
  const auto n=bus_->blocks.front();bus_->blocks.pop_front();h={};h.block_sequence=n;h.first_frame=n*1024;h.source_generation[0]=s_.session_id[0];
  for(unsigned c=0;c<s_.channels;++c)for(unsigned f=0;f<1024;++f)planes[c][f]=sample(n,c,f);
  return ReadResult::Block;
 }
};}
namespace entropy::input {
enum class State:unsigned {Disabled,Waiting,Receiving,Fault,OwnershipBusy};enum class Domain {Music,Cinema};
struct Telemetry {
 State state=State::Disabled;uint64_t received=0,consumed=0,empty=0,faults=0,generation=0;
 unsigned queued=0,producerPid=0,validityMask=0,sourceDiscontinuityMask=0;
 uint64_t lastBlockQpc=0,releaseFailures=0,gaps=0,lastSequence=0,lastFirstFrame=0,lastEpoch=0,sourceQpc=0,sourceFlaggedBlocks=0;
 std::array<uint8_t,16> sourceSession{};
};
struct DomainTelemetry:Telemetry {StageBusSessionHeader session{};StageBusBlockHeader block{};uint64_t discarded=0,maximumReceiveGapQpc=0,maximumSourceAgeQpc=0;std::array<double,8> peaks{},maximumPeaks{};};
struct DomainContext {StageBusSessionHeader session{};StageBusBlockHeader block{};uint64_t token=0;};
struct GraphInputContext {DomainContext music{},cinema{};uint32_t domains=0;};
bool localEndpoint(const std::wstring& s) noexcept{return !s.empty();}
std::wstring defaultEndpoint(Domain d){return d==Domain::Music?L"music":L"cinema";}
bool validateSessionForDomain(const StageBusSessionHeader& s,Domain) noexcept{return s.channels==3||s.channels==6||s.channels==8;}
bool validateBlockForDomain(const StageBusSessionHeader&,const StageBusBlockHeader& h,Domain) noexcept{return h.frames==1024;}
class DomainIngress {
public: // Expose fixture internals only; production declarations stay unchanged.
 DomainIngress();~DomainIngress();bool start(Domain,std::wstring);void stop() noexcept;void restart() noexcept;void required(bool) noexcept;
 DomainTelemetry telemetry() const noexcept;State state() const noexcept;bool tryRead(double* const*,StageBusBlockHeader&) noexcept;
 struct Impl;std::unique_ptr<Impl> impl_;bool acquire() noexcept;void release() noexcept;bool ready() const noexcept;void readLocked(double* const*,DomainContext&) noexcept;
};
class ProgramIngress {
public:
 ~ProgramIngress(){stop();}bool start(std::wstring={},std::wstring={},bool=true);void stop() noexcept;void restartSubscription(Domain) noexcept;bool configure(std::wstring,std::wstring);
 bool tryRead(double* const*,uint32_t,GraphInputContext&) noexcept;void require(uint32_t d) noexcept{music_.required(d&1);cinema_.required(d&2);}
 DomainIngress music_,cinema_;void* owner_=nullptr;
};
}
'''
CASES = r'''
using namespace entropy::input;
struct Audio {std::array<std::array<double,1024>,13> pcm{};std::array<double*,13> planes{};Audio(){for(unsigned c=0;c<13;++c)planes[c]=pcm[c].data();}};
void verify(const Audio& a,uint64_t n,unsigned channels,unsigned offset=0){for(unsigned c=0;c<channels;++c)for(unsigned f=0;f<1024;++f)check(std::bit_cast<uint64_t>(a.pcm[c+offset][f])==std::bit_cast<uint64_t>(sample(n,c,f)),"PCM changed");}
void workerCases(unsigned channels){setupBus(channels);DomainIngress d;d.required(true);check(d.start(Domain::Music,L"music"),"worker start");until([]{return buses[0].connects.load()!=0;},"connect");Audio a;StageBusBlockHeader h;
 std::atomic<bool> done{false};std::thread observer([&]{while(!done.load())(void)d.telemetry();});
 for(uint64_t n=1;n<=64;++n){send(n);until([&]{return d.tryRead(a.planes.data(),h);},"worker read");check(h.block_sequence==n&&h.first_frame==n*1024,"source identity");verify(a,n,channels);}
 done=true;observer.join();check(d.telemetry().consumed==64&&d.telemetry().faults==0,"worker counters");d.stop();std::cout<<"PASS exact worker body "<<channels<<"ch under telemetry\n";
}
void inactiveAndOverflow(){setupBus(3);DomainIngress d;check(d.start(Domain::Music,L"music"),"idle start");until([]{return buses[0].connects.load()!=0;},"idle connect");
 for(uint64_t n=1;n<=40;++n){send(n);until([&]{return d.telemetry().received==n;},"idle block");}
 auto t=d.telemetry();check(t.queued==1&&t.discarded==39&&t.faults==0,"inactive keep-latest");d.required(true);Audio a;StageBusBlockHeader h;until([&]{return d.tryRead(a.planes.data(),h);},"activate");verify(a,40,3);
 for(uint64_t n=41;n<=56;++n){send(n);until([&]{return d.telemetry().received==n;},"fill");}
 check(d.telemetry().queued==16,"same 16-slot capacity");buses[0].allowConnect=false;send(57);
 until([&]{return d.telemetry().faults>0;},"overflow must fault");check(d.telemetry().gaps==1&&d.telemetry().queued==0,"overflow not hidden/replayed");d.stop();std::cout<<"PASS idle keep-latest and active overflow fault\n";
}
void reconnectPinned(){setupBus(3);DomainIngress d;d.required(true);check(d.start(Domain::Music,L"music"),"restart start");until([]{return buses[0].connects.load()!=0;},"restart connect");send(1);until([&]{return d.telemetry().received==1;},"first block");
 check(d.acquire(),"pin old session");check(d.ready(),"old ready");const auto* old=d.impl_->queue.front();auto generation=d.telemetry().generation;
 {std::lock_guard g(buses[0].mutex);buses[0].session.session_id[0]=2;}
 auto closed=buses[0].closes.load();d.restart();until([&]{return buses[0].closes.load()>closed;},"retirement requested");Sleep(2);
 check(old->block.block_sequence==1&&d.impl_->session.session_id[0]==1,"no overwrite while pinned");d.release();
 until([&]{return buses[0].connects.load()==2&&d.telemetry().session.session_id[0]==2;},"new session published");
 check(d.telemetry().queued==0&&d.telemetry().generation>generation,"old queue retired");send(2);Audio a;StageBusBlockHeader h;until([&]{return d.tryRead(a.planes.data(),h);},"new session read");verify(a,2,3);d.stop();std::cout<<"PASS reconnect cannot overwrite pinned old session/PCM\n";
}
void inject(DomainIngress& d,unsigned channels,uint64_t n){auto& i=*d.impl_;i.session.channels=channels;i.session.session_id[0]=1;i.state=State::Receiving;auto* p=i.queue.producerSlot();check(p,"inject slot");p->block.block_sequence=n;p->block.first_frame=n*1024;p->token=9;for(unsigned c=0;c<channels;++c)for(unsigned f=0;f<1024;++f)p->audio[c][f]=sample(n,c,f);i.queue.commitProducer();}
void paired(){ProgramIngress p;Audio a;GraphInputContext c;inject(p.music_,3,1);
 check(!p.tryRead(a.planes.data(),3,c),"paired read must wait for both");check(p.music_.impl_->queue.readableApprox()==1,"no half consumption");inject(p.cinema_,6,2);
 check(p.tryRead(a.planes.data(),3,c),"paired read");verify(a,1,3);verify(a,2,6,3);check(c.domains==3&&c.music.token==9&&c.cinema.token==9,"paired metadata");
 inject(p.music_,3,3);check(p.cinema_.impl_->queue.beginMaintenance(),"cinema maintenance");check(p.tryRead(a.planes.data(),1,c),"MUSIC independent of CINEMA lifecycle");verify(a,3,3);p.cinema_.impl_->queue.endMaintenance();
 inject(p.cinema_,8,4);check(p.music_.impl_->queue.beginMaintenance(),"music maintenance");check(p.tryRead(a.planes.data(),2,c),"CINEMA independent of MUSIC lifecycle");verify(a,4,8,3);p.music_.impl_->queue.endMaintenance();std::cout<<"PASS exact ProgramIngress paired/independent-domain methods\n";
}
int main(){try{workerCases(3);workerCases(6);workerCases(8);inactiveAndOverflow();reconnectPinned();paired();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
'''

def main():
    compiler = os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        raise SystemExit('C++20 compiler required; no packages are installed by this script.')
    domain = (ROOT / 'Engine/Input/DomainIngress.cpp').read_text()
    program = (ROOT / 'Engine/Input/ProgramIngress.cpp').read_text()
    telemetry = domain.split('DomainTelemetry DomainIngress::telemetry()', 1)[1].split('bool DomainIngress::acquire()', 1)[0]
    assert 'i.lock()' not in telemetry and 'i.observation.read()' in telemetry
    assert 'impl_->queue.acquireConsumer()' in domain
    with tempfile.TemporaryDirectory(prefix='entropy-r2-') as folder:
        temp = Path(folder)
        adapter = temp / 'domain-adapter.cpp'
        strip_includes = lambda s: re.sub(r'^#include[^\n]*\n', '', s, flags=re.M)
        adapter.write_text(STUBS + strip_includes(domain) + strip_includes(program) + CASES)
        flags = ['-std=c++20', '-O2', '-fno-fast-math', '-pthread', '-I', str(ROOT)]
        for name, source in [('queue', ROOT / 'Tests/RecoveryR2QueueTests.cpp'), ('adapter', adapter)]:
            binary = temp / name
            subprocess.run([compiler, *flags, str(source), '-o', str(binary)], check=True, timeout=45)
            subprocess.run([str(binary)], check=True, timeout=30)
    print('PASS focused portable R2 checks; Windows/MSVC/IPC/DAC not tested.')

if __name__ == '__main__':
    main()
