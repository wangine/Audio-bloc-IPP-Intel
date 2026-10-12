#include "DspRecorder.h"
#include <fstream>
#include <cstring>
#include <memory>
#include <chrono>
namespace entropy::stage {
bool DspRecorder::start(const std::filesystem::path& path,uint64_t limit,unsigned delay){
 if(active_ || !limit || limit>11250)return false;
 if(writer_.joinable())writer_.join();
 auto file=std::make_shared<std::ofstream>(path,std::ios::binary|std::ios::trunc);if(!*file)return false;
 write_=read_=attempts_=dropped_=written_=0;stopping_=failed_=false;limit_=limit;active_=true;
 try{writer_=std::thread([this,file,delay](){
  while(!stopping_ || read_.load(std::memory_order_acquire)!=write_.load(std::memory_order_acquire)){
   auto r=read_.load(std::memory_order_relaxed),w=write_.load(std::memory_order_acquire);
   if(r==w){if(emptyQueueProbe_)emptyQueueProbe_(probeContext_);if(attempts_.load(std::memory_order_acquire)>=limit_ && r==write_.load(std::memory_order_acquire))break;std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
   auto& slot=queue_[r&3];if(delay)std::this_thread::sleep_for(std::chrono::milliseconds(delay));
   file->write(reinterpret_cast<const char*>(&slot.header),128);file->write(reinterpret_cast<const char*>(slot.data.data()),3*8192);
   if(!*file){failed_=true;active_=false;}else written_++;
   read_.store(r+1,std::memory_order_release);
  }
  file->flush();if(!*file)failed_=true;active_=false;
 });}catch(...){active_=false;return false;}return true;
}
void DspRecorder::stop() noexcept{active_=false;stopping_=true;if(writer_.joinable())writer_.join();}
bool DspRecorder::tryCapture(const StageBusBlockHeader& b,const double* const* planes) noexcept {
 if(!active_.load(std::memory_order_acquire) || attempts_>=limit_)return false;
 auto w=write_.load(std::memory_order_relaxed),r=read_.load(std::memory_order_acquire);
 if(!planes || b.frames!=1024 || w-r>=4){dropped_++;attempts_.fetch_add(1,std::memory_order_release);return false;}
 for(unsigned c=0;c<3;c++)if(!planes[c]){dropped_++;attempts_.fetch_add(1,std::memory_order_release);return false;}
 auto& slot=queue_[w&3];slot.header=b;for(unsigned c=0;c<3;c++)std::memcpy(slot.data[c].data(),planes[c],8192);
 write_.store(w+1,std::memory_order_release);attempts_.fetch_add(1,std::memory_order_release);return true;
}
}
