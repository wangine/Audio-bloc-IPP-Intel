#include "PcmBridgeV1.h"
namespace entropy::music::bridge {
DecodeResult validate(const Packet& p,uint32_t wireBytes,uint32_t capacity) noexcept {
    uint32_t bytes=0;
    if(p.header.magic!=Magic || p.header.version!=Version || p.header.kind!=1 ||
        !payloadSize(p.format,p.descriptor.frames,bytes) || p.payloadBytes!=bytes ||
        p.header.totalBytes!=sizeof(p)+bytes || wireBytes!=p.header.totalBytes || p.reserved || p.reserved2 ||
        !p.descriptor.generation || !p.descriptor.sequence || !p.descriptor.qpcFrequency || p.descriptor.reserved ||
        p.descriptor.firstSourceFrame>UINT64_MAX-p.descriptor.frames || p.descriptor.mode>2 ||
        !(p.descriptor.flags&RawIdentity) || (p.descriptor.flags&~(RawIdentity|Discontinuity|EndOfStream)))return DecodeResult::Invalid;
    return capacity<wireBytes?DecodeResult::BufferTooSmall:DecodeResult::Valid;
}
}
