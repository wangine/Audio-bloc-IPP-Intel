#include "CinemaOutput.h"
#include "CinemaDeadlineWait.h"
#include "CaptureScheduling.h"
#include "StagePublishV2.h"
#include "StageBusValidation.h"
#include "StageLinkSecurity.h"
#include <windows.h>
#include <cstring>
#include <thread>
#include <cmath>
#include <optional>
bool CinemaBlockProcessor::prepare(unsigned channels){
 if(channels!=6&&channels!=8)return false;
 channels_=channels;if(!src_.prepare(256,128))return false;
 for(auto& p:scratch_)p.resize(src_.outputCapacity());reset();return true;
}
void CinemaBlockProcessor::reset() noexcept {fill_=assembled_=0;src_.configure(48000);}
bool CinemaBlockProcessor::push(const std::array<float,8>& f,Callback callback,void* context) noexcept {
 if(!channels_||!callback)return false;
 for(unsigned c=0;c<8;c++){if(!std::isfinite(f[c]))return false;input_[c][fill_]=c<channels_?double(f[c]):0.;}
 if(++fill_!=256)return true;fill_=0;
 std::array<const double*,8> in{};std::array<double*,8> out{};for(unsigned c=0;c<8;c++){in[c]=input_[c].data();out[c]=scratch_[c].data();}
 unsigned made=0;if(!src_.process(in.data(),256,out.data(),src_.outputCapacity(),made))return false;
 for(unsigned i=0;i<made;i++){
  for(unsigned c=0;c<8;c++)block_[c][assembled_]=out[c][i];
  if(++assembled_==1024){std::array<const double*,8> p{};for(unsigned c=0;c<8;c++)p[c]=block_[c].data();callback(context,p.data());assembled_=0;}
 }return true;
}
struct CinemaOutput::Impl {
 static constexpr uint64_t capacity=32768;
 struct Slot{std::array<float,8> frame{};uint64_t epoch=0;};
 std::vector<Slot> queue{capacity};
 // One atomic word keeps generation and validity coherent with admission.
 // Low bit: active. Remaining bits: generation (starts at one).
 std::atomic<uint64_t> write{0},read{0},state{2},drops{0},published{0};
 std::atomic<bool> running{false},waitFailed{false};std::atomic_flag admission=ATOMIC_FLAG_INIT;
 CinemaBlockProcessor processor;entropy::publish::Publisher publisher;std::thread worker;CinemaDeadlineWait wait;
 StageBusSessionHeader session{};uint64_t workerEpoch=0,sequence=0;unsigned channels=0;LARGE_INTEGER frequency{};
 void retire(uint64_t token) noexcept {auto expected=token*2+1;state.compare_exchange_strong(expected,(token+1)*2,std::memory_order_acq_rel);}
 void newSession(uint64_t e){GUID id{};CoCreateGuid(&id);session=entropy::stage::makeSession(channels==6?CINEMA_51_6_V1:CINEMA_71_8_V1,reinterpret_cast<const uint8_t*>(&id),frequency.QuadPart);session.producer_pid=GetCurrentProcessId();publisher.setSession(session);processor.reset();workerEpoch=e;sequence=0;}
 static void block(void* ctx,const double* const* p){auto& s=*static_cast<Impl*>(ctx);if(s.state.load(std::memory_order_acquire)!=(s.workerEpoch*2+1))return;
  StageBusBlockHeader b{};std::memcpy(b.session_id,s.session.session_id,16);b.stream_epoch=s.workerEpoch;b.block_sequence=++s.sequence;b.first_frame=(s.sequence-1)*1024;b.control_revision=1;b.source_generation[2]=s.workerEpoch;b.source_state[2]=1;b.source_rate[2]=48000;b.frames=1024;b.validity_mask=4;b.discontinuity_mask=s.sequence==1?4:0;LARGE_INTEGER q{};QueryPerformanceCounter(&q);b.qpc_timestamp=q.QuadPart;s.publisher.publish(b,p);s.published.fetch_add(1,std::memory_order_relaxed);
 }
 void loop(){
  // PCM can be paced on time while this downstream owner is descheduled.
  // Register on the publishing owner, retaining all existing queue/deadline policy.
  std::optional<CaptureScheduling> audioScheduling;
  try{audioScheduling.emplace();}
  catch(...){retire(state.load(std::memory_order_acquire)>>1);waitFailed=true;running=false;publisher.stop();return;}
  while(running.load(std::memory_order_acquire)){
   auto e=state.load(std::memory_order_acquire)>>1;if(e!=workerEpoch)newSession(e);
   publisher.service();unsigned budget=4096;bool worked=false;
   while(budget--){const auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))break;const auto slot=queue[r&(capacity-1)];read.store(r+1,std::memory_order_release);worked=true;
    e=state.load(std::memory_order_acquire)>>1;if(e!=workerEpoch)newSession(e);if(slot.epoch!=e)continue;
    if(!processor.push(slot.frame,block,this)){drops.fetch_add(1);retire(e);break;}
   }
   if(!worked&&!wait.forDuration(std::chrono::milliseconds(1))){retire(e);waitFailed=true;running=false;}
  }publisher.stop();}
};
CinemaOutput::CinemaOutput():impl_(std::make_unique<Impl>()){}
CinemaOutput::~CinemaOutput(){stop();}
std::wstring CinemaOutput::defaultEndpoint(){auto n=entropy::link::endpointName();const auto pos=n.find(L"StageLink.V1");if(pos!=std::wstring::npos)n.replace(pos,12,L"StagePublish.Cinema.Decoder.V2");return n;}
bool CinemaOutput::start(unsigned channels,const std::wstring& endpoint,std::string& error){auto& s=*impl_;if(s.running||s.worker.joinable()){error="Already started; call stop before restart";return false;}if(!s.wait.valid()){error="High resolution publication timer unavailable";return false;}if(!s.processor.prepare(channels)){error="SRC prepare failed";return false;}s.waitFailed=false;s.channels=channels;s.write=0;s.read=0;s.state.store(((s.state.load()>>1)+1)*2);QueryPerformanceFrequency(&s.frequency);if(!s.publisher.start(endpoint,error))return false;s.newSession(s.state.load()>>1);s.running=true;s.worker=std::thread([&s]{s.loop();});return true;}
void CinemaOutput::invalidate() noexcept{auto& s=*impl_;auto current=s.state.load(std::memory_order_acquire);while(!s.state.compare_exchange_weak(current,((current>>1)+1)*2,std::memory_order_acq_rel)){} }
void CinemaOutput::suspend() noexcept{auto& s=*impl_;auto current=s.state.load(std::memory_order_acquire);while(current&1){if(s.state.compare_exchange_weak(current,((current>>1)+1)*2,std::memory_order_acq_rel))break;}}
void CinemaOutput::submit(const std::array<float,8>& frame) noexcept{submit(frame,generation());}
void CinemaOutput::submit(const std::array<float,8>& frame,uint64_t token) noexcept{auto& s=*impl_;if(!s.running)return;
 if((s.state.load(std::memory_order_acquire)>>1)!=token)return;
 if(s.admission.test_and_set(std::memory_order_acquire)){s.drops.fetch_add(1);s.retire(token);return;}
 const auto w=s.write.load(std::memory_order_relaxed);if(w-s.read.load(std::memory_order_acquire)>=Impl::capacity){s.drops.fetch_add(1);s.retire(token);s.admission.clear(std::memory_order_release);return;}
 auto current=s.state.load(std::memory_order_acquire);
 if((current>>1)!=token){s.admission.clear(std::memory_order_release);return;}
 if(!(current&1)&&!s.state.compare_exchange_strong(current,token*2+1,std::memory_order_acq_rel)){s.admission.clear(std::memory_order_release);return;}
 s.queue[w&(Impl::capacity-1)]={frame,token};s.write.store(w+1,std::memory_order_release);s.admission.clear(std::memory_order_release);
}
void CinemaOutput::stop() noexcept{auto& s=*impl_;s.running=false;if(s.worker.joinable())s.worker.join();}
uint64_t CinemaOutput::dropped() const noexcept{return impl_->drops.load()+impl_->publisher.dropped();}
uint64_t CinemaOutput::blocks() const noexcept{return impl_->published.load();}
uint64_t CinemaOutput::generation() const noexcept{return impl_->state.load(std::memory_order_acquire)>>1;}
bool CinemaOutput::schedulingFailed() const noexcept{return impl_->waitFailed.load();}


