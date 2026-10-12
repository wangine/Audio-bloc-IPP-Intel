#pragma once
#include "StageBusWin32.h"
#include <array>
#include <cstring>
#include <iostream>
#include <windows.h>
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL "<<__FILE__<<":"<<__LINE__<<" "<<#x<<"\n";return 1;} }while(0)
using namespace entropy::stage;
inline uint64_t qpc(){LARGE_INTEGER v;QueryPerformanceCounter(&v);return v.QuadPart;}
inline uint64_t frequency(){LARGE_INTEGER v;QueryPerformanceFrequency(&v);return v.QuadPart;}
inline StageBusSessionHeader session(unsigned layout){uint8_t uuid[16]{};auto p=GetCurrentProcessId();auto t=qpc();std::memcpy(uuid,&t,8);std::memcpy(uuid+8,&p,4);uuid[15]=17;return makeSession(layout,uuid,frequency());}
inline StageBusFrameRequest request(const StageBusSessionHeader& s,uint64_t seq){StageBusFrameRequest r{};std::memcpy(r.session_id,s.session_id,16);r.stream_epoch=1;r.block_sequence=seq;r.first_frame=(seq-1)*1024;r.frames=1024;r.control_revision=1;r.program.required_inputs=1;r.program.music_program_active=1;return r;}
inline StageBusBlockHeader header(const StageBusFrameRequest& r){StageBusBlockHeader b{};std::memcpy(b.session_id,r.session_id,16);b.stream_epoch=r.stream_epoch;b.block_sequence=r.block_sequence;b.first_frame=r.first_frame;b.frames=r.frames;b.control_revision=r.control_revision;b.source_generation[0]=7;b.source_generation[1]=9;b.source_generation[2]=13;b.music_session_revision=11;b.validity_mask=7;b.qpc_timestamp=qpc();return b;}
template<size_t N> struct Planes{std::array<std::array<double,1024>,N> data{};std::array<double*,N> out{};std::array<const double*,N> in{};Planes(){for(size_t c=0;c<N;c++){out[c]=data[c].data();in[c]=out[c];}}};
inline uint64_t bits(double v){uint64_t x;std::memcpy(&x,&v,8);return x;}
