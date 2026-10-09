#pragma once

#include "CrossoverControlLedger.h"

#include <cstdint>

namespace entropy::host
{
enum class ProjectLoadAckState : std::uint8_t
{
    idle,
    busy,
    loaded,
    failed
};

enum class ProjectLoadAckReason : std::uint8_t
{
    none,
    crossoverRejected,
    crossoverSuperseded,
    legacyUnconfirmed,
    loudnessFault,
    parametricEqFault
};

struct ProjectLoadAckObservation
{
    CrossoverCommandSnapshot crossover{};
    std::uint64_t loudnessAppliedSequence = 0;
    std::uint64_t parametricEqAppliedSequence = 0;
    bool crossoverEndpointAvailable = false;
    bool loudnessFault = false;
    bool parametricEqFault = false;
};

class ProjectLoadAckGate final
{
public:
    bool begin(std::uint64_t projectSequence,
               CrossoverCommandId crossoverCommand,
               std::uint64_t loudnessSequence,
               std::uint64_t parametricEqSequence) noexcept;
    void observe(const ProjectLoadAckObservation& observation) noexcept;

    ProjectLoadAckState state() const noexcept { return state_; }
    ProjectLoadAckReason reason() const noexcept { return reason_; }
    std::uint64_t projectSequence() const noexcept { return projectSequence_; }
    bool active() const noexcept { return state_ == ProjectLoadAckState::busy; }

private:
    bool optionalAcksComplete(const ProjectLoadAckObservation& observation) const noexcept;

    std::uint64_t projectSequence_ = 0;
    CrossoverCommandId crossoverCommand_{};
    std::uint64_t loudnessSequence_ = 0;
    std::uint64_t parametricEqSequence_ = 0;
    ProjectLoadAckState state_ = ProjectLoadAckState::idle;
    ProjectLoadAckReason reason_ = ProjectLoadAckReason::none;
};
}
