#include "Format.h"
#include <string.h>
namespace entropy::music {
bool validateFormat(const PcmFormat& f) noexcept {
    const bool rate=f.sampleRate==44100 || f.sampleRate==48000 || f.sampleRate==88200 ||
        f.sampleRate==96000 || f.sampleRate==176400 || f.sampleRate==192000;
    const bool bits=(f.containerBits==16 && f.validBits==16) || (f.containerBits==32 && f.validBits==24);
    return f.structSize==sizeof(f) && rate && bits && f.channels==2 && f.channelMask==3 &&
        f.blockAlign==f.containerBits/4 && f.averageBytesPerSecond==f.sampleRate*f.blockAlign &&
        f.formatTag==0xfffe && f.representation==1 && !memcmp(f.subFormat,PcmGuid,16) &&
        !f.reserved[0] && !f.reserved[1];
}
bool payloadSize(const PcmFormat& f,uint32_t frames,uint32_t& bytes) noexcept {
    if(!validateFormat(f) || !frames || frames>MaxPacketFrames) return false;
    bytes=frames*f.blockAlign;return true;
}
}
