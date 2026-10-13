"""Bounded Linux/macOS test of the exact ProgramIngress::tryRead body.
Windows transport/locks are replaced by a deterministic fixture. This does not
claim to compile the Windows Core or test audio-device timing.
Usage: python3 Tests/RecoveryR1Portable.py [source-root]
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
compiler = shutil.which('g++') or shutil.which('clang++')
if not compiler:
    raise SystemExit('A C++20 compiler (g++ or clang++) is required.')
text = (root / 'Engine/Input/ProgramIngress.cpp').read_text(encoding='utf-8')
start = text.index('bool ProgramIngress::tryRead(')
brace = text.index('{', start)
depth = 1
end = brace + 1
while depth:
    if text[end] == '{': depth += 1
    if text[end] == '}': depth -= 1
    end += 1
body = text[start:end]
fixture = r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
struct DomainContext { unsigned id=0; };
struct GraphInputContext { DomainContext music{},cinema{}; uint32_t domains=0; };
struct FakeDomain {
 bool busy=false,available=true,locked=false;
 unsigned attempts=0,reads=0,releases=0,channels=3,id=1;
 bool acquire() noexcept { ++attempts;if(busy)return false;locked=true;return true; }
 bool ready() const noexcept {return available;}
 void release() noexcept {if(!locked)std::abort();locked=false;++releases;}
 void readLocked(double* const* out,DomainContext& c) noexcept {
  if(!locked||!available)std::abort();
  ++reads;c.id=id;
  for(unsigned ch=0;ch<channels;++ch)for(unsigned f=0;f<1024;++f)
   out[ch][f]=(f==0?-0.0:double(id*100+ch)+double(f)/2048.0);
 }
};
struct ProgramIngress {
 FakeDomain music_,cinema_;
 ProgramIngress(){cinema_.channels=8;cinema_.id=2;}
 void require(uint32_t) noexcept {}
 bool tryRead(double* const*,uint32_t,GraphInputContext&) noexcept;
};
'''
checks = r'''
int main(){
 unsigned failures=0,checks=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 std::array<std::array<double,1024>,13> data{};std::array<double*,13> out{};
 for(unsigned c=0;c<13;++c)out[c]=data[c].data();
 GraphInputContext context{};
 {ProgramIngress p;p.cinema_.busy=true;
  const bool ok=p.tryRead(out.data(),1,context);
  check(ok && p.music_.reads==1 && p.cinema_.attempts==0,"MUSIC ignores occupied CINEMA lock");
  check(ok && context.domains==1 && std::bit_cast<uint64_t>(data[0][0])==(uint64_t{1}<<63),"MUSIC signed zero copied exactly");
  check(ok && data[2][1023]==102.0+1023.0/2048.0 && data[3][17]==0,"MUSIC payload and inactive lanes");}
 {ProgramIngress p;p.music_.busy=true;
  check(p.tryRead(out.data(),2,context)&&p.cinema_.reads==1&&p.music_.attempts==0,"CINEMA ignores occupied MUSIC lock");
  check(context.domains==2 && data[3][37]==200.0+37.0/2048.0 && data[10][1023]==207.0+1023.0/2048.0,"CINEMA offset and payload");}
 {ProgramIngress p;p.cinema_.busy=true;
  check(!p.tryRead(out.data(),3,context)&&p.music_.reads==0&&p.cinema_.reads==0&&!p.music_.locked,"dual-domain contention consumes neither");}
 {ProgramIngress p;p.cinema_.available=false;
  check(!p.tryRead(out.data(),3,context)&&p.music_.reads==0&&!p.music_.locked&&!p.cinema_.locked,"dual-domain empty queue consumes neither");}
 {ProgramIngress p;
  check(p.tryRead(out.data(),3,context)&&p.music_.reads==1&&p.cinema_.reads==1&&context.domains==3,"dual-domain successful read");}
 {ProgramIngress p;
  check(!p.tryRead(out.data(),0,context)&&!p.tryRead(out.data(),4,context)&&p.music_.attempts==0&&p.cinema_.attempts==0,"invalid domain mask rejected");}
 {ProgramIngress p;auto bad=out;bad[12]=nullptr;
  check(!p.tryRead(bad.data(),1,context)&&p.music_.attempts==0,"invalid output rejected before dequeue");}
 std::printf("ProgramIngress fixture: %u/%u checks passed\n",checks-failures,checks);return failures?1:0;
}
'''
with tempfile.TemporaryDirectory(prefix='entropy-r1-') as tmp:
    cpp = Path(tmp)/'ingress.cpp'
    cpp.write_text(fixture+'\n'+body+'\n'+checks,encoding='utf-8')
    exe = Path(tmp)/'ingress'
    subprocess.run([compiler,'-std=c++20','-O2','-Wall','-Wextra','-include','cstdlib',str(cpp),'-o',str(exe)],check=True)
    result = subprocess.run([str(exe)],check=False)
    if result.returncode:
        raise SystemExit(result.returncode)

# Exact queue-copy fragment in a portable byte-preservation fixture.
s=(root/'Engine/Input/DomainIngress.cpp').read_text()
start=s.index('   scratch.token=stats.generation;');end=s.index('   stats.block=',start)
code=s[start:end]
fixture=r'''
#include <array>
#include <cstdint>
#include <cstring>
#include <bit>
#include <cstdio>
struct Slot {std::array<uint64_t,16> block{};uint64_t token=0;std::array<std::array<double,1024>,8> audio{};};
int main(){unsigned checks=0,failures=0;
for(unsigned n:{3u,6u,8u})for(unsigned h:{0u,15u}){
 struct {unsigned channels;}session{n};struct {uint64_t generation;}stats{77};
 std::array<Slot,16> queue{};Slot scratch{};unsigned head=h,size=0;
 for(unsigned c=0;c<8;++c)for(unsigned f=0;f<1024;++f){
  // Finite binary64 payload, including negative zero.
  const uint64_t bits=f==0?(uint64_t{1}<<63):(uint64_t{0x3fe0000000000000}|(uint64_t(c)<<24)|f);
  scratch.audio[c][f]=std::bit_cast<double>(bits);queue[head].audio[c][f]=-123.0;
 }
 for(unsigned i=0;i<16;++i)scratch.block[i]=i+99;
'''+code+r'''
 bool ok=size==1&&queue[head].token==77&&queue[head].block==scratch.block;
 for(unsigned c=0;c<n;++c)ok=ok&&std::memcmp(queue[head].audio[c].data(),scratch.audio[c].data(),1024*sizeof(double))==0;
 ++checks;if(!ok)++failures;
}
std::printf("Queue copy: %u/%u bitwise checks passed\n",checks-failures,checks);return failures?1:0;}
'''
with tempfile.TemporaryDirectory(prefix='entropy-copy-') as t:
 p=Path(t);(p/'test.cpp').write_text(fixture)
 subprocess.run([compiler,'-std=c++20','-O2',str(p/'test.cpp'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True)
# This is a source-scope check, not a scheduler performance benchmark.
locked=s[s.index('const auto receivedQpc=tick();lock();'):s.index('++stats.received;state=State::Receiving;unlock();')]
assert 'for(double v:scratch.audio[c])' not in locked,'peak scan still holds queue guard'
print('Queue guard: no per-sample peak scan under lock')

# Source/timestamp checks, not a real driver timing test.
import re
s=(root/'Engine/Egress/LiveSinkWorkers.cpp').read_text()
# A deterministic event schedule: wait returned at t=1000000; setup and
# calibration take 90000 ticks; render/submit then takes 10000 ticks.
# Test the actual timestamp-selection expression in run(), not a rewritten DSP.
begin=re.search(r'const auto begin=([^;]+);ULONG64',s).group(1)
values={'host100ns()':1090000,'waitReturn100ns':1000000}
assert begin in values, f'unreviewed timestamp expression: {begin}'
measured=(1100000-values[begin])/10
checks=[(measured==10000,'callback metric includes pre-render work'),
        (s.index('std::memcpy(audit->rendered[ch]')>s.index('const auto submitResult=sink->submit(block);'),'capture copy occurs after submit'),
        ('estimatedLateSubmissions' in s and 'submitEnd100ns' in s,'separate estimated hardware deadline evidence is exposed')]
for ok,label in checks:print(('PASS ' if ok else 'FAIL ')+label)
raise SystemExit(0 if all(ok for ok,_ in checks) else 1)
