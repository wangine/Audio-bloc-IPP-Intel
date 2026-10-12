#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <thread>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <memory>
#include <vector>
#include <stdexcept>
#include "../../Transport/StageBusSDK/include/entropy/stage_bus_v1.h"
// Optional observation only. Prepared on the lifecycle thread before Graph starts.
// Every producer owns a separate bounded SPSC queue; disk I/O belongs to workers.
namespace entropy::chain_audit {
struct GraphRecord {
 uint64_t qpc{},graphFrame{},token{},drops{};StageBusBlockHeader source{};
 double input[13][1024]{},crossover[16][1024]{},matrix[16][1024]{},output[16][1024]{};
};
struct SinkRecord {
 uint64_t qpc{},deviceFrame{},host100ns{},drops{},resync{},begin100ns{},end100ns{};
 double firstGraph{},endGraph{},ppm{},clockFill{};
 uint32_t frames{},channels{},format{},audio{},offset{},fifo{},faults{},resetReason{},accepted{},submitResult{};
 double rendered[16][1024]{};uint8_t pcm[16*1024*4]{};
};
template<class T> class Recorder {
 static constexpr size_t capacity=4096;
 std::vector<T> slots_;std::atomic<uint64_t> write_{0},read_{0};
 std::atomic<bool> stopping_{false};std::thread worker_;uint64_t attempts_=0,drops_=0;
 std::filesystem::path folder_;std::string name_;bool failed_=false;
 void run(){
  uint64_t written=0,bytes=0;unsigned part=0;std::ofstream file;
  auto open=[&]{file.open(folder_/(name_+"-"+std::to_string(part++)+".bin"),std::ios::binary);bytes=0;};open();
  while(!stopping_.load(std::memory_order_acquire)||read_.load()!=write_.load()){
   auto r=read_.load(std::memory_order_relaxed);if(r==write_.load(std::memory_order_acquire)){Sleep(1);continue;}
   file.write(reinterpret_cast<const char*>(&slots_[r%capacity]),sizeof(T));if(!file)failed_=true;else ++written;
   bytes+=sizeof(T);read_.store(r+1,std::memory_order_release);
   if(bytes>=512ULL*1024*1024){file.flush();if(!file)failed_=true;file.close();if(file.fail())failed_=true;open();}
  }
  file.flush();if(!file)failed_=true;file.close();if(file.fail())failed_=true;std::ofstream summary(folder_/(name_+"-summary.json"));summary<<"{\"recordBytes\":"<<sizeof(T)<<",\"attempts\":"<<attempts_<<",\"written\":"<<written<<",\"drops\":"<<drops_<<",\"failed\":"<<(failed_?"true":"false")<<"}\n";
 }
public:
 Recorder(const std::filesystem::path& folder,std::string name):slots_(capacity),folder_(folder),name_(std::move(name)){worker_=std::thread([this]{run();});}
 ~Recorder(){stopping_.store(true,std::memory_order_release);if(worker_.joinable())worker_.join();}
 T* reserve() noexcept {
  if(attempts_>=11250)return nullptr;++attempts_;auto w=write_.load(std::memory_order_relaxed);
  if(w-read_.load(std::memory_order_acquire)>=capacity){++drops_;return nullptr;}auto* s=&slots_[w%capacity];s->drops=drops_;return s;
 }
 void commit() noexcept {write_.fetch_add(1,std::memory_order_release);}
};
struct Session {std::unique_ptr<Recorder<GraphRecord>> graph;std::array<std::unique_ptr<Recorder<SinkRecord>>,2> sink;};
inline std::unique_ptr<Session> session;
struct Stamp {StageBusBlockHeader source{};uint64_t token{},graphFrame{};};
inline thread_local Stamp stamp;
inline uint64_t tick() noexcept {LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
inline void prepare(){
 auto* value=_wgetenv(L"ENTROPY_CHAIN_AUDIT_DIR");if(!value||!*value)return;
 auto s=std::make_unique<Session>();std::filesystem::path folder(value);if(std::filesystem::exists(folder)&&!std::filesystem::is_empty(folder))throw std::runtime_error("Chain audit requires a fresh empty directory");std::filesystem::create_directories(folder);
 s->graph=std::make_unique<Recorder<GraphRecord>>(folder,"graph");
 for(unsigned i=0;i<2;++i)s->sink[i]=std::make_unique<Recorder<SinkRecord>>(folder,i?"dac-b":"dac-a");session=std::move(s);
}
inline void context(const StageBusBlockHeader& header,uint64_t token,uint64_t graphFrame) noexcept {if(session){stamp.source=header;stamp.token=token;stamp.graphFrame=graphFrame;}}
inline void graph(const double* const* in,const double* const* cross,const double* const* matrix,const double* const* output,unsigned frames) noexcept {
 if(!session||frames!=1024)return;auto& writer=*session->graph;auto* r=writer.reserve();if(!r)return;
 r->qpc=tick();r->graphFrame=stamp.graphFrame;r->token=stamp.token;r->source=stamp.source;
 for(unsigned i=0;i<13;++i)std::memcpy(r->input[i],in[i],8192);
 for(unsigned i=0;i<16;++i){std::memcpy(r->crossover[i],cross[i],8192);std::memcpy(r->matrix[i],matrix[i],8192);std::memcpy(r->output[i],output[i],8192);}writer.commit();
}
inline SinkRecord* sink(unsigned slot) noexcept {return session&&slot<2?session->sink[slot]->reserve():nullptr;}
inline void sinkDone(unsigned slot) noexcept {session->sink[slot]->commit();}
inline void finish(){session.reset();}
}
