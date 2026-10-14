// Bounded offline tests for the new memory-to-packet boundary. No Windows/DAC.
#include "CoreFile.h"
#include <array>
#include <iostream>
#include <vector>
#include <stdexcept>
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main() try {
 unsigned cases=0;
 for(unsigned channels:{1u,2u,8u,16u})for(unsigned bytes:{3u,4u})for(unsigned quantum:{1u,7u,256u,1024u,2048u,4096u}){
  const size_t stride=channels*bytes,frames=10007;
  std::vector<uint8_t> source(frames*stride),packet(quantum*stride),result;
  for(size_t k=0;k<source.size();++k)source[k]=uint8_t((k*131u+k/17u)&255u);
  const auto before=b1::hash(source);b1::Cursor c(source,stride);
  while(!c.done()){
   auto old=c.position();require(c.stage(packet,quantum),"stage rejected");
   require(c.position()==old,"stage advanced before submit");
   require(!c.stage(packet,quantum),"double stage accepted");
   require(!c.commit(quantum+1),"wrong accepted-frame count accepted");
   require(c.position()==old,"failed commit moved cursor");
   auto count=std::min<uint64_t>(quantum,frames-old);
   result.insert(result.end(),packet.begin(),packet.begin()+size_t(count)*stride);
   for(size_t n=size_t(count)*stride;n<packet.size();++n)require(packet[n]==0,"tail not zero");
   require(c.commit(quantum),"commit rejected");require(!c.commit(quantum),"double commit accepted");
  }
  require(result==source,"packetization changed/dropped/repeated bytes");
  require(b1::hash(source)==before,"input mutated");
  require(c.stage(packet,quantum)&&c.commit(quantum),"drain packet failed");
  for(auto v:packet)require(v==0,"drain not silent");require(c.position()==frames,"drain advanced source");
  ++cases;
 }
 std::array<uint8_t,12> a{};b1::Cursor c(a,6);
 std::array<uint8_t,5> small{};require(!c.stage(small,1),"undersized destination accepted");
 require(!c.stage(a,0),"zero request accepted");
 auto header=b1::Header{};header.channels=2;header.containerBits=32;header.frames=2;header.channelMask=3;
 require(b1::payloadBytes(header)==16,"header byte count");
 bool rejected=false;try{header.frames=UINT64_MAX;(void)b1::payloadBytes(header);}catch(...){rejected=true;}
 require(rejected,"oversized header accepted");
 std::cout<<"PASS "<<cases<<" packetization cases plus admission/header checks; no hardware qualification\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
