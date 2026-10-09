#pragma once
#include "ProjectPersistence.h"
#include <entropy/loudness_abi.h>
namespace entropy::host {
// Non-realtime orchestration only. Call restore last in a project transaction;
// a refused import has no side effects. Host reserves its command mailbox until ACK.
class LoudnessProjectState final {
    EntropyModuleHandle handle_{};
    EntropyLoudnessApiV1 api_{};
    ProjectModuleState fresh_;
    ProjectModuleState retainedAbsent_;
public:
    bool initialize(EntropyModuleHandle,const EntropyLoudnessApiV1&,const std::string& version);
    bool available() const noexcept {return handle_!=nullptr;}
    bool capture(ProjectModuleState&) const;
    bool validate(const ProjectModuleState&) const;
    bool restore(const ProjectModuleState&,uint64_t& sequence);
    bool applied(uint64_t sequence) const;
    void serviceStopped() const {if(handle_&&api_.service_stopped)api_.service_stopped(handle_);}
};
}
