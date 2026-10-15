#include "../model/ClockFollow.h"
#include "../protocol/ClockBridgeV1.h"
#include <stdexcept>
#include <iostream>
using namespace entropy::music;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void runLeaseContracts(){
 constexpr uint64_t hz=10000000,start=123;
 Clock c;ClockFollow follow;check(c.reset(48000,hz,3)&&c.start(start),"lease clock");
 check(follow.acquire(c,start,2,17,9)==FollowResult::Stale,"wrong source generation");
 check(follow.acquire(c,start,3,17,9)==FollowResult::Ok,"lease acquire");
 check(follow.acquire(c,start,3,18,9)==FollowResult::Busy,"second controller");
 check(follow.update(c,start+hz/2,3,17,9,1,100000)==FollowResult::Ok,"rate update");
 auto s=follow.snapshot(c,start+hz/2);check(s.appliedPpb==10000&&s.requestedPpb==100000,"20 ppm/second limit");
 uint64_t before=0,after=0;check(c.position(start+hz,before),"before rejection");
 check(follow.update(c,start+hz,3,17,9,1,-100000)==FollowResult::Stale,"replayed sequence");
 check(follow.update(c,start+hz,3,17,10,2,-100000)==FollowResult::Stale,"wrong master epoch");
 check(follow.update(c,start+hz,3,18,9,2,-100000)==FollowResult::Stale,"wrong handle lease");
 check(c.position(start+hz,after)&&before==after,"rejected control moved phase");
 const auto expiry=start+hz*5/2;
 s=follow.snapshot(c,expiry);check(s.state==FollowState::Expired&&s.appliedPpb==0,"expiry status");
 check(follow.update(c,expiry,3,17,9,3,10)==FollowResult::Stale,"expired lease renewed silently");
 check(follow.acquire(c,expiry,3,19,10)==FollowResult::Ok,"fresh lease after expiry");
 check(!follow.releaseHandle(c,expiry,17),"old handle canceled new controller");
 check(follow.releaseHandle(c,expiry,19),"owner release");
 check(follow.snapshot(c,expiry).state==FollowState::Autonomous,"release state");
 namespace w=clockwire;
 w::Request r{{w::Magic,w::Version,sizeof(w::Request),uint32_t(w::Kind::Acquire)},3,0,12,0,0,0};
 check(w::validate(r,w::ioctl(w::Kind::Acquire),sizeof(r),sizeof(w::Reply)),"valid ABI request");
 r.reserved=1;check(!w::validate(r,w::ioctl(w::Kind::Acquire),sizeof(r),sizeof(w::Reply)),"reserved validation");r.reserved=0;
 check(!w::validate(r,w::ioctl(w::Kind::Acquire),sizeof(r)-1,sizeof(w::Reply)),"length validation");
 check(!w::validate(r,w::ioctl(w::Kind::Update),sizeof(r),sizeof(w::Reply)),"kind/code validation");
 const auto reply=w::reply(w::Kind::Status,s);check(reply.header.totalBytes==128&&reply.qpcFrequency==hz,"reply ABI");
 std::cout<<"ClockFollow lease/ABI contracts PASS; source generation, ownership, replay, bounds and expiry.\n";
}
