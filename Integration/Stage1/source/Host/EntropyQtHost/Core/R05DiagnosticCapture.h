#pragma once
#include "R05NumericalDiagnostics.h"
#include <entropy/r04_diagnostics.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <string>
#include <windows.h>
namespace entropy::host {
inline std::uint64_t r05Qpc()noexcept{LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
class R05DiagnosticCapture final {
 entropy::r04::Queue<entropy::r05::Packet,128> producer_;
 std::array<entropy::r04::Queue<entropy::r05::Packet,32>,2> render_;
 std::atomic<bool> stop_{false};std::thread worker_;
 std::filesystem::path path_;std::ofstream stages_,mapping_,status_;
 void run()noexcept;void write(const entropy::r05::Packet&);
public:
 ~R05DiagnosticCapture(){stop();}
 bool start(const std::filesystem::path&,std::string&);
 void stop()noexcept{stop_=true;if(worker_.joinable())worker_.join();}
 bool publishProducer(const entropy::r05::Packet& p)noexcept{return producer_.push(p);}
 bool publishRender(unsigned domain,const entropy::r05::Packet& p)noexcept{return domain<render_.size()&&render_[domain].push(p);}
 const auto& path()const noexcept{return path_;}
};
// One owner per recorder. No formatting, I/O, Qt, locks or allocation in begin/observe/finish.
class R05BlockRecorder final {
 std::array<entropy::r05::Analyzer,entropy::r05::stageCount> analyzers_{};
 entropy::r05::Packet packet_{};
 std::array<std::uint64_t,3> generation_{};bool haveGeneration_=false;
public:
 void begin(entropy::r05::Block block,std::uint32_t domain=0,std::uint64_t session=0)noexcept{
  if(haveGeneration_&&generation_!=block.generation)resetHistory();generation_=block.generation;haveGeneration_=true;
  packet_={};packet_.block=block;packet_.frames=block.frames;packet_.domain=domain;packet_.session=session;
 }
 void resetHistory()noexcept{for(auto& a:analyzers_)a.reset();}
 void observe(entropy::r05::Stage stage,const double* const* inputs,unsigned channels,unsigned frames,unsigned flags=0,const std::uint32_t* lanes=nullptr)noexcept{
  const auto qpc=r05Qpc();for(unsigned c=0;c<(std::min)(channels,entropy::r05::maxChannels);++c){
   if(packet_.count>=packet_.events.size()){packet_.fault=UINT32_MAX;return;}
   auto& e=packet_.events[packet_.count++];e.stage=stage;e.channel=c;e.lane=lanes?lanes[c]:c;e.flags=flags;e.qpc=qpc;
   e.metric=analyzers_[unsigned(stage)].measure(c,inputs?inputs[c]:nullptr,frames);
  }
 }
 entropy::r05::Packet& packet()noexcept{return packet_;}
};
}
