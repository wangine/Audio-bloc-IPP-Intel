#include "../model/Clock.h"
#include <iostream>
#include <stdexcept>
#include <cstdint>
using entropy::music::Clock;
static void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class C> void followContracts(){
 if constexpr(requires(C& c){c.discipline(uint64_t{},int32_t{},uint64_t{});}) {
  constexpr uint64_t hz=10000000,start=1000000;
  C c;require(c.reset(192000,hz,1)&&c.start(start),"start");
  uint64_t p=0,before=0;
  require(c.position(start+hz,before)&&before==192000,"nominal");
  require(c.discipline(start+hz,100000,start+hz*4),"100 ppm admission");
  require(c.position(start+hz,p)&&p==before,"phase changed on update");
  require(c.position(start+hz*3,p)&&p==576038,"100 ppm two-second position");
  require(c.position(start+hz*5,p)&&p==960057,"expired lease must use nominal slope after deadline");
  require(!c.discipline(start+hz*5,1000001,start+hz*6),"range rejection");
  uint64_t unchanged=0;require(c.position(start+hz*5,unchanged)&&unchanged==p,"rejected update changed phase");
  require(c.discipline(start+hz*5,-100000,start+hz*8),"negative correction");
  require(c.position(start+hz*5,unchanged)&&unchanged==p,"negative update phase jump");
  require(c.position(start+hz*7,p)&&p==1344019,"negative correction fractional carry");
  require(c.pause(start+hz*7),"pause");
  require(c.position(start+hz*100,unchanged)&&unchanged==p,"paused clock moved");
  require(c.start(start+hz*100),"resume");
  require(c.position(start+hz*101,unchanged)&&unchanged==p+192000,"pause must invalidate correction");
  require(c.reset(44100,hz,2)&&c.start(start),"new generation");
  require(c.position(start+hz,p)&&p==44100,"reset must be nominal");
  std::cout<<"Clock follow: continuity, signs, expiry, pause/reset PASS\n";
 } else throw std::runtime_error("Clock has no external fine-rate actuator; only nominal QPC pacing exists");
}
int main(){try{
 constexpr uint64_t hz=10000000,start=177;
 for(uint32_t rate:{44100u,48000u,88200u,96000u,176400u,192000u}){
  Clock c;require(c.reset(rate,hz,9)&&c.start(start),"nominal setup");
  uint64_t accumulated=0,p=0;
  for(uint64_t step=1;step<=1000;++step){
   const uint64_t span=step*1739;accumulated+=span;
   require(c.pause(start+step*hz+span),"pause sequence");
   // This independent case tests initial run plus subsequent varied segments.
   uint64_t expectedTicks=hz+accumulated;
   require(c.position(start+step*hz+span,p),"position while paused");
   uint64_t expected=0;require(entropy::music::multiplyDivide(expectedTicks,rate,hz,expected),"reference");
   require(p==expected,"nominal fractional remainder lost across PAUSE");
   require(c.start(start+(step+1)*hz),"next run");
  }
 }
 followContracts<Clock>();
 std::cout<<"ClockFollow C1 model contracts PASS; no driver, pipe or audio device opened.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
