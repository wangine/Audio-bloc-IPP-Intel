#pragma once
#include "SourceDomainRuntime.h"

namespace entropy::host {
struct ProgramSourcePoll {
    SourceBlock1024 music, aux, cinema;
    bool musicAvailable = false, auxAvailable = false, cinemaAvailable = false;
};

// The program gate selects downstream audio, not ownership of input capture.
template<class Sources>
ProgramSourcePoll pollProgramSources(Sources& sources, bool ready, bool cinemaProgram,
                                     unsigned requiredInputs = 0) noexcept {
    ProgramSourcePoll result;
    const bool music = ready && sources.popTidal1024(result.music);
    const bool aux = ready && sources.popAux1024(result.aux);
    const bool cinema = ready && sources.popCinema1024(result.cinema);
    // Preserve contributors through an adopted ramp. Zero retains legacy gating.
    const unsigned mask = requiredInputs != 0 ? requiredInputs : (cinemaProgram ? 2u : 1u);
    result.musicAvailable = music && (mask & 1u) != 0;
    result.auxAvailable = aux && (mask & 1u) != 0;
    result.cinemaAvailable = cinema && (mask & 2u) != 0;
    return result;
}
}
