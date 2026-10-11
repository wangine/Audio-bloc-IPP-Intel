#pragma once
#include "../model/PacketTimeline.h"
#include <stddef.h>
namespace entropy::music::bridge {
constexpr uint32_t Magic=0x324d5045, Version=0x00010000;
// FILE_DEVICE_UNKNOWN, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA.
constexpr uint32_t IoctlRead=(0x22u<<16)|(3u<<14)|(0x800u<<2);
constexpr uint32_t IoctlInfo=(0x22u<<16)|(3u<<14)|(0x801u<<2);
constexpr uint32_t IoctlClaim=(0x22u<<16)|(3u<<14)|(0x802u<<2);
constexpr uint32_t IoctlTrace=(0x22u<<16)|(3u<<14)|(0x803u<<2);
enum class TraceEvent:uint32_t {OutputInterface,WriteCall,WriteSuccess,WriteFailure,Position,Presentation,PacketCount,Notification};
constexpr wchar_t HardwareId[]=L"ROOT\\EntropyMusicRenderV2";
constexpr wchar_t ServiceName[]=L"EntropyMusicRenderV2";
constexpr char ProviderId[]="entropy.music.render.v2.raw";
constexpr uint8_t InterfaceGuid[16]={0x9e,0x28,0x93,0xab,0x92,0x26,0xa9,0x47,0xbc,0x47,0xed,0x17,0x11,0xf6,0xe4,0x30};
constexpr wchar_t ReferenceName[]=L"EntropyMusicPcmV1";
constexpr uint32_t CoreTransportAccess=8;
struct Header {uint32_t magic, version, totalBytes, kind;};
struct Trace {Header header;uint64_t counters[8];uint32_t lastPacketNumber,lastFlags,lastEosBytes,lastStatus;};
static_assert(sizeof(Trace)==96);
struct Packet {
    Header header;
    PcmFormat format;
    PacketDescriptor descriptor;
    uint32_t payloadBytes, reserved;
    uint64_t reserved2;
};
static_assert(sizeof(Header)==16 && sizeof(Packet)==160);
static_assert(offsetof(Packet,format)==16 && offsetof(Packet,descriptor)==80);
struct Claim {Header header;uint64_t nonce,reserved;};
struct Info {
    Header header;
    uint8_t interfaceGuid[16];
    PcmFormat format;
    uint64_t generation,qpcFrequency,sourceFrames,packetCount,committedFrames,publishedFrames,deliveredFrames,lostFrames,cancelledFrames,currentUnderrunFrames;
    uint32_t allocationMode,state,readerAttached,driverBuild;
    uint64_t terminatedLost,terminatedCancelled,lifetimeLost,lifetimeCancelled;
    uint64_t reserved[4];
};
static_assert(sizeof(Claim)==32 && sizeof(Info)==256);
enum class DecodeResult {Valid,Invalid,BufferTooSmall};
DecodeResult validate(const Packet&,uint32_t wireBytes,uint32_t readCapacity) noexcept;
}
