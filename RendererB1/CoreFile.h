#pragma once
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
namespace b1 {
static_assert(std::endian::native==std::endian::little,"B1 file format is little endian");
inline constexpr uint64_t magic=UINT64_C(0x314d435031425445);
inline constexpr uint64_t hashSeed=UINT64_C(14695981039346656037);
struct Header {
 uint64_t tag=magic;
 uint32_t version=1,rate=192000,channels=0,containerBits=32;
 uint64_t frames=0,payloadHash=hashSeed;
 uint32_t channelMask=0,reserved0=0;
 uint64_t reserved[2]{};
};
static_assert(sizeof(Header)==64);
inline uint64_t hash(std::span<const uint8_t> bytes,uint64_t value=hashSeed) noexcept {
 for(auto b:bytes)value=(value^b)*UINT64_C(1099511628211);return value;
}
inline uint64_t payloadBytes(const Header& h){
 if(h.tag!=magic||h.version!=1||h.rate!=192000||!h.channels||h.channels>16||
    (h.containerBits!=24&&h.containerBits!=32)||!h.frames||h.reserved0||h.reserved[0]||h.reserved[1])
  throw std::runtime_error("Invalid B1 PCM header");
 const uint64_t stride=uint64_t(h.channels)*(h.containerBits/8);
 if(h.frames>(UINT64_C(2)*1024*1024*1024)/stride)throw std::runtime_error("B1 PCM exceeds 2 GiB limit");
 return h.frames*stride;
}
// Single render owner. Stage is non-advancing; commit follows successful ReleaseBuffer.
class Cursor {
 std::span<const uint8_t> source_;size_t stride_;uint64_t position_=0;
 unsigned pendingFrames_=0,pendingSource_=0;
public:
 Cursor(std::span<const uint8_t> source,size_t stride):source_(source),stride_(stride){
  if(!stride||source.empty()||source.size()%stride)throw std::invalid_argument("Invalid frame-aligned PCM");
 }
 uint64_t position() const noexcept{return position_;}
 uint64_t frames() const noexcept{return source_.size()/stride_;}
 bool done() const noexcept{return position_==frames();}
 bool stage(std::span<uint8_t> destination,unsigned request) noexcept {
  if(!request||pendingFrames_||uint64_t(request)*stride_>destination.size())return false;
  const auto count=unsigned(std::min<uint64_t>(request,frames()-position_));
  const auto actual=size_t(count)*stride_,total=size_t(request)*stride_;
  if(actual)std::memcpy(destination.data(),source_.data()+size_t(position_)*stride_,actual);
  if(total>actual)std::memset(destination.data()+actual,0,total-actual);
  pendingFrames_=request;pendingSource_=count;return true;
 }
 bool commit(unsigned accepted) noexcept {
  if(!pendingFrames_||accepted!=pendingFrames_)return false;
  position_+=pendingSource_;pendingFrames_=pendingSource_=0;return true;
 }
};
}
