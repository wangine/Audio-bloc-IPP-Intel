#include "PcmVerifier.h"
namespace entropy::music::audit {
bool PcmVerifier::accept(const EntropyIngressPacketV2& p,const uint8_t* bytes){
    if(!bytes || p.abiVersion!=0x20000 || p.structSize!=160 || !validateFormat(format_) || std::memcmp(&format_,&p.format,64) ||
       p.flags!=RawIdentity || p.reserved[0] || p.reserved[1] || !p.generation || !p.qpcFrequency || p.frames>MaxPacketFrames ||
       p.payloadBytes!=p.frames*format_.blockAlign)return reject("invalid raw PCM metadata");
    result_.lostFrames+=p.lostFrames;if(p.lostFrames)return reject("driver reports lost frames");
    if(p.firstSourceFrame!=next_ || p.packetSequence!=sequence_+1)return reject("missing, duplicate or reordered source frames/packets");
    if(generation_ && (generation_!=p.generation || p.qpcTimestamp<qpc_))return reject("unexpected generation or QPC reversal");
    if(next_>expected_ || p.frames>expected_-next_)return reject("extra tail frames");
    uint8_t expected[8]{};for(uint32_t i=0;i<p.frames;++i){PcmPattern::frame(next_+i,format_,expected);for(uint32_t b=0;b<format_.blockAlign;++b)if(expected[b]!=bytes[i*format_.blockAlign+b])++result_.mismatchedBytes;}
    next_+=p.frames;result_.frames=next_;sequence_=p.packetSequence;generation_=p.generation;qpc_=p.qpcTimestamp;
    if(result_.mismatchedBytes)return reject("PCM bytes differ");return result_.reason.empty();
}
Comparison PcmVerifier::finish(bool footer,uint64_t overflow,uint64_t failedWrites,bool drained){
    if(!footer)reject("missing audit footer");if(overflow || failedWrites)reject("audit overflow or pipe failure");
    if(!drained)reject("interrupted stream; tail not drained");if(next_!=expected_)reject("incomplete first-to-last frame coverage");
    result_.pass=result_.reason.empty();return result_;
}
}
