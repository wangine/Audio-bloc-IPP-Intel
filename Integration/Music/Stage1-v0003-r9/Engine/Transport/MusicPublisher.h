#pragma once
#include "SourceDomain.h"
#include <entropy/stage_bus_v1.h>
#include <memory>
#include <string>
namespace entropy::music {
struct ProgramBlock {
    StereoBlock audio;
    SourceBlockStatus sources[2]{};
    uint32_t validity{}, discontinuity{};
    uint64_t controlRevision = 1, musicRevision = 1;
};
class MusicPublisher {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    MusicPublisher();
    ~MusicPublisher();
    bool start(const std::wstring &, std::string &);
    void service() noexcept;
    void publish(const ProgramBlock &) noexcept;
    void stop();
    uint64_t sequence() const noexcept;
    uint64_t dropped() const noexcept;
    const StageBusSessionHeader &session() const noexcept;
};
} // namespace entropy::music
