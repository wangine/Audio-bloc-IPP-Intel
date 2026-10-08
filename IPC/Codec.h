#pragma once
#include "Protocol.h"
#include <algorithm>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
namespace entropy::ipc {
inline constexpr std::uint32_t kMaximumFrameBytes=1024*1024;
struct Frame {std::uint32_t command=0,requestId=0;std::string payload;};
inline std::uint32_t load32(const unsigned char* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
inline void store32(unsigned char* p,std::uint32_t v){for(unsigned i=0;i<4;++i)p[i]=static_cast<unsigned char>(v>>(8*i));}
inline std::vector<unsigned char> encode(const Frame& f){
    if(f.payload.size()>kMaximumFrameBytes-16)throw std::invalid_argument("IPC frame too large");
    std::vector<unsigned char> bytes(f.payload.size()+16);store32(bytes.data(),kProtocolVersion);store32(bytes.data()+4,static_cast<uint32_t>(bytes.size()));store32(bytes.data()+8,f.command);store32(bytes.data()+12,f.requestId);
    std::copy(f.payload.begin(),f.payload.end(),bytes.begin()+16);return bytes;
}
class Framer {
    std::vector<unsigned char> buffer_;std::vector<Frame> frames_;
public:
    void push(std::span<const unsigned char> bytes){
        if(bytes.size()+buffer_.size()>kMaximumFrameBytes*2)throw std::invalid_argument("IPC input buffer too large");buffer_.insert(buffer_.end(),bytes.begin(),bytes.end());
        while(buffer_.size()>=16){
            auto version=load32(buffer_.data()),size=load32(buffer_.data()+4);
            if(version!=kProtocolVersion||size<16||size>kMaximumFrameBytes)throw std::invalid_argument("IPC version/length mismatch");
            if(buffer_.size()<size)break;
            frames_.push_back({load32(buffer_.data()+8),load32(buffer_.data()+12),std::string(buffer_.begin()+16,buffer_.begin()+size)});
            if(frames_.size()>4096)throw std::invalid_argument("IPC frame queue full");
            buffer_.erase(buffer_.begin(),buffer_.begin()+size);
        }
    }
    std::vector<Frame> take(){std::vector<Frame> out;out.swap(frames_);return out;}
};
}
