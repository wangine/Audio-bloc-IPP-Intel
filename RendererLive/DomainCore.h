#pragma once
#include "AudioQueues.h"
#include "RateControl.h"
#include "WdlClockAsrc.h"
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <array>
#include <atomic>
#include <vector>
namespace entropy::live {
// Graph -> input queue -> preparation owner -> ready queue -> sink owner.
class DomainCore final {
public:
 enum class Production {Made,Waiting,Full,Fault};
 bool prepare(unsigned channels,unsigned format,uint64_t seed,bool deterministic=false) {
  if(!channels||channels>16||format>1||(!deterministic&&!seed))return false;
  channels_=channels;format_=format;
  if(!input.prepare(channels,65536)||!ready.prepare(channels,format?32:24,4096,4)||!asrc_.prepare(channels,4096))return false;
  interleaved_.assign(size_t(channels)*4096,0.0);planar_.assign(size_t(channels)*4096,0.0);
  for(unsigned c=0;c<channels;++c)planes_[c]=planar_.data()+size_t(c)*4096;
  EntropyPcm24Config config{};config.struct_size=sizeof(config);config.revision=1;config.channels=channels;
  config.maximum_frames=4096;config.output_format=format;config.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;
  config.seed_mode=deterministic?ENTROPY_PCM24_SEED_DETERMINISTIC_TEST:ENTROPY_PCM24_SEED_PRODUCTION_INDEPENDENT;config.seed=seed;
  config.reserved_u32=ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT;
  if(!pcm_.configure(config))return false;
  submittedSource_=preparedSource_=0;submittedFrames_=0;ratio_=1.0;appliedPpm_=requestedPpm_=level_=0;rateUpdates_=0;return true;
 }
 bool setQuantum(unsigned frames) noexcept {
  if(!frames||frames>4096)return false;
  quantum_=frames;target_=(std::max)(8192u,frames*4u);servo_.prepare(target_);return true;
 }
 Production produce() noexcept {
  auto* packet=ready.beginWrite();if(!packet)return Production::Full;
  const auto ratio=ratio_.load(std::memory_order_acquire);
  if(!asrc_.process(input,interleaved_.data(),quantum_,ratio))return asrc_.healthy()?Production::Waiting:Production::Fault;
  for(unsigned c=0;c<channels_;++c)for(unsigned f=0;f<quantum_;++f){const double v=interleaved_[size_t(f)*channels_+c];if(!std::isfinite(v))return Production::Fault;planar_[size_t(c)*4096+f]=v;}
  EntropyPcm24BlockF64 block{};block.struct_size=sizeof(block);block.channels=channels_;block.frames=quantum_;
  block.inputs=planes_.data();block.output_interleaved=packet->bytes.data();block.output_capacity_bytes=bytesPerPacket();
  if(pcm_.process(block)!=ENTROPY_OK||block.produced_bytes!=bytesPerPacket())return Production::Fault;
  packet->frames=quantum_;packet->sourceBegin=asrc_.beginPosition();packet->sourceEnd=asrc_.endPosition();
  preparedSource_.store(packet->sourceEnd,std::memory_order_release);appliedPpm_.store((ratio-1.0)*1e6,std::memory_order_relaxed);
  return ready.commitWrite()?Production::Made:Production::Fault;
 }
 // Only after successful ReleaseBuffer; packet bytes must remain pinned until then.
 bool submitted() noexcept {
  const auto* packet=ready.beginRead();if(!packet)return false;
  const double end=packet->sourceEnd;const unsigned frames=packet->frames;
  const double backlog=double(input.published())-end;
  if(!std::isfinite(backlog)||backlog< -2.0)return false;
  servo_.observe((std::max)(0.0,backlog),frames);
  ratio_.store(servo_.ratio(),std::memory_order_release);requestedPpm_.store(servo_.requestedPpm(),std::memory_order_relaxed);
  level_.store(backlog,std::memory_order_relaxed);rateUpdates_.store(servo_.updates(),std::memory_order_relaxed);
  submittedSource_.store(end,std::memory_order_release);submittedFrames_.fetch_add(frames,std::memory_order_release);
  return ready.commitRead();
 }
 unsigned quantum() const noexcept {return quantum_;}
 unsigned target() const noexcept {return target_;}
 unsigned primeFrames() const noexcept {return target_+quantum_;}
 unsigned bytesPerPacket() const noexcept {return quantum_*channels_*(format_?4u:3u);}
 double pendingSourceFrames() const noexcept {return (std::max)(0.0,double(input.published())-submittedSource_.load(std::memory_order_acquire));}
 double preparedSource() const noexcept {return preparedSource_.load(std::memory_order_acquire);}
 double submittedSource() const noexcept {return submittedSource_.load(std::memory_order_acquire);}
 uint64_t submittedFrames() const noexcept {return submittedFrames_.load(std::memory_order_acquire);}
 double appliedPpm() const noexcept {return appliedPpm_.load(std::memory_order_relaxed);}
 double requestedPpm() const noexcept {return requestedPpm_.load(std::memory_order_relaxed);}
 double observedLevel() const noexcept {return level_.load(std::memory_order_relaxed);}
 uint64_t rateUpdates() const noexcept {return rateUpdates_.load(std::memory_order_relaxed);}
 EntropyPcm24Telemetry pcmTelemetry() const noexcept {return pcm_.telemetry();}
 AudioQueue input;ReadyQueue ready;
private:
 unsigned channels_=0,format_=0,quantum_=0,target_=8192;
 WdlClockAsrc asrc_;pcm24::Pcm24ModuleEngine pcm_;RateControl servo_;
 std::vector<double> interleaved_,planar_;std::array<const double*,16> planes_{};
 std::atomic<double> submittedSource_{0},preparedSource_{0},ratio_{1.0},appliedPpm_{0},requestedPpm_{0},level_{0};
 std::atomic<uint64_t> submittedFrames_{0},rateUpdates_{0};
};
}
