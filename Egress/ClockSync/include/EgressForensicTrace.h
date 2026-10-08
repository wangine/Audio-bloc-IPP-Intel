#pragma once
#include "ForensicQueue.h"
#include <entropy/egress_forensic_abi.h>
#include <chrono>
namespace entropy::egress {
// B2-FF-006 TEMP_FORENSIC: this header is included only by a trace-ON adapter.
// State belongs to the original module instance. No worker/thread/filesystem here.
inline std::uint64_t forensicNowNs() noexcept {
 const auto n=std::chrono::duration_cast<std::chrono::nanoseconds>(
  std::chrono::steady_clock::now().time_since_epoch()).count();
 return n>0?static_cast<std::uint64_t>(n):0;
}
class EgressForensicTrace final {
public:
 static constexpr std::uint32_t capacity=4096;
 void push(EntropyForensicEvent value) noexcept {
  value.struct_size=sizeof(value);value.kind=ENTROPY_FORENSIC_PUSH;
  value.sequence=++pushSequence_;push_.push(value);
 }
 void render(EntropyForensicEvent value) noexcept {
  value.struct_size=sizeof(value);value.kind=ENTROPY_FORENSIC_RENDER;
  value.sequence=++renderSequence_;render_.push(value);
 }
 EntropyForensicStatus status()const noexcept {
  EntropyForensicStatus s{};s.struct_size=sizeof(s);s.version=ENTROPY_EGRESS_FORENSIC_VERSION;
  s.enabled=1;s.queue_capacity=capacity;s.now_ns=forensicNowNs();
  s.push_dropped=push_.dropped();s.render_dropped=render_.dropped();
  s.push_written=push_.written();s.render_written=render_.written();return s;
 }
 std::uint32_t drain(std::uint32_t kind,EntropyForensicEvent* out,std::uint32_t cap) noexcept {
  if(!out || !cap || cap>capacity || (kind!=1 && kind!=2))return 0;
  std::uint32_t n=0;
  if(kind==1){while(n<cap && push_.pop(out[n]))++n;}
  else {while(n<cap && render_.pop(out[n]))++n;}
  return n;
 }
 void resetQuiescent()noexcept {push_.resetQuiescent();render_.resetQuiescent();pushSequence_=renderSequence_=0;}
private:
 ForensicQueue<EntropyForensicEvent,capacity> push_,render_;
 std::uint64_t pushSequence_=0,renderSequence_=0; // each counter has its own writer
};
}
