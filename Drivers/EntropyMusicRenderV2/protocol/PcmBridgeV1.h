#pragma once
#include "../model/PacketTimeline.h"
#include <stddef.h>
namespace entropy::music::bridge {
constexpr uint32_t Magic=0x324d5045, Version=0x00010000;
// FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA.
constexpr uint32_t IoctlRead=(0x22u<<16)|(3u<<14)|(0x800u<<2);
constexpr uint32_t IoctlInfo=(0x22u<<16)|(3u<<14)|(0x801u<<2);
constexpr uint32_t IoctlClaim=(0x22u<<16)|(3u<<14)|(0x802u<<2);
constexpr wchar_t HardwareId[]=L"ROOT\\EntropyMusicRenderV2";
constexpr wchar_t ServiceName[]=L"EntropyMusicRenderV2";
constexpr char ProviderId[]="entropy.music.render.v2.raw";
struct Header {uint32_t magic, version, totalBytes, kind;};
struct Packet {
    Header header;
    PcmFormat format;
    PacketDescriptor descriptor;
    uint32_t payloadBytes, reserved;
    uint64_t reserved2;
};
static_assert(sizeof(Header)==16 && sizeof(Packet)==160);
static_assert(offsetof(Packet,format)==16 && offsetof(Packet,descriptor)==80);
enum class DecodeResult {Valid,Invalid,BufferTooSmall};
DecodeResult validate(const Packet&,uint32_t wireBytes,uint32_t readCapacity) noexcept;
}
