#pragma once
#include "Engine/Input/ProgramIngress.h"
#include "StageBusValidation.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace entropy;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct Source {
 publish::Publisher publisher;StageBusSessionHeader session{};std::wstring endpoint;
 std::array<std::array<double,1024>,8> audio{};std::array<const double*,8> planes{};
 std::atomic<bool> stopping{false},silence{false},invalid{false},skip{false};std::atomic<uint64_t> sequence{9000};std::thread worker;
 std::atomic<uint64_t> musicRevision{91};std::atomic<uint32_t> generation{23};
 std::atomic<bool> infinity{false};unsigned periodMs=5;
 Source(unsigned layout,unsigned id,unsigned revision=0,double scale=1.0,unsigned period=5):periodMs(period) {
  LARGE_INTEGER q{};QueryPerformanceFrequency(&q);uint8_t uuid[16]={uint8_t(id),2,3,uint8_t(revision)};session=stage::makeSession(layout,uuid,q.QuadPart);session.producer_pid=GetCurrentProcessId();
  endpoint=input::defaultEndpoint(layout==4?input::Domain::Music:input::Domain::Cinema)+L".test."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(id);
  for(unsigned c=0;c<8;++c){planes[c]=audio[c].data();for(unsigned f=0;f<1024;++f)audio[c][f]=scale*double(c*1024+f)/17.;audio[c][0]=-0.0;}
  std::string error;check(publisher.start(endpoint,error),"fixture publisher start");publisher.setSession(session);
  worker=std::thread([this]{while(!stopping){publisher.service();auto seq=sequence.fetch_add(skip.exchange(false)?2:1);StageBusBlockHeader b{};memcpy(b.session_id,session.session_id,16);b.stream_epoch=7;b.block_sequence=seq;b.first_frame=170000+(seq-9000)*1024;b.control_revision=11;b.frames=1024;LARGE_INTEGER q{};QueryPerformanceCounter(&q);b.qpc_timestamp=q.QuadPart;
   const bool music=session.layout_id==4;b.validity_mask=music?3:4;b.source_generation[music?0:2]=generation.load();b.source_rate[music?0:2]=44100;if(music)b.music_session_revision=musicRevision.load();
   std::array<std::array<double,1024>,8> special{};std::array<const double*,8> selected=planes;
   if(silence.load()||invalid.load()||infinity.load()){if(invalid.load())special[0][0]=std::numeric_limits<double>::quiet_NaN();if(infinity.load())special[0][0]=std::numeric_limits<double>::infinity();for(unsigned c=0;c<8;++c)selected[c]=special[c].data();}
   publisher.publish(b,selected.data());Sleep(periodMs);}});
 }
 ~Source(){stopping=true;worker.join();publisher.stop();}
};

