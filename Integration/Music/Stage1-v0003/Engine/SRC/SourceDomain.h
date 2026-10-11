#pragma once
#include "PcmDecode.h"
#include <array>
#include <atomic>
#include <memory>
namespace entropy::music {
struct SourceBlockStatus {
    uint64_t generation{}, sequence{}, firstFrame{}, lostFrames{};
    uint32_t rate{}, flags{}, mode{}, containerBits{}, validBits{};
};
struct StereoBlock {
    std::array<double, 1024> left{}, right{};
    SourceBlockStatus status{};
};
struct SourceTelemetry {
    uint64_t generation{}, packets{}, framesIn{}, framesOut{}, lostFrames{}, overruns{}, underruns{},
        faults{}, packetSequence{}, firstSourceFrame{}, qpcTimestamp{}, qpcFrequency{},
        startupFramesMeasured{};
    uint32_t rate{}, mode{}, containerBits{}, validBits{}, fillFrames{}, representation{}, metadataFlags{},
        truePeakReady{}, startupComplete{};
    double truePeakL{}, truePeakR{}, startupPeakL{}, startupPeakR{}, prePeakL{}, prePeakR{}, postPeakL{},
        postPeakR{}, preRmsL{}, preRmsR{}, preMidRms{}, preSideRms{}, postRmsL{}, postRmsR{}, postMidRms{},
        postSideRms{};
};
class SourceDomain {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourceDomain();
    ~SourceDomain();
    SourceDomain(const SourceDomain &) = delete;
    bool prepare(const EntropyIngressFormatV2 &, uint64_t generation,
                 uint32_t maximumFrames = ENTROPY_MAX_PACKET_FRAMES);
    using WaitForSpace = bool (*)(void *) noexcept;
    bool processPacket(const IngressPacketView &, WaitForSpace wait = nullptr,
                       void *waitContext = nullptr) noexcept;
    bool pop1024(StereoBlock &) noexcept;
    void setIngressRunning(bool) noexcept;
    bool popClocked1024(StereoBlock &,uint64_t now=0,uint64_t frequency=0) noexcept;
    uint32_t queuedFrames() const noexcept;
    SourceTelemetry telemetry() const noexcept;
    void invalidate(bool clearTelemetry=true) noexcept;
};
} // namespace entropy::music
