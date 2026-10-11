#pragma once

namespace entropy::standalone {

#define ENTROPY_CROSSOVER_16(BASE) \
    BASE "1", BASE "2", BASE "3", BASE "4", BASE "5", BASE "6", BASE "7", BASE "8", \
    BASE "9", BASE "10", BASE "11", BASE "12", BASE "13", BASE "14", BASE "15", BASE "16"

inline constexpr const char* parameterIds[] = {
    "cinemaSource", "cinemaScoopSource", "cinemaLfeReferenceGain", "globalDelayMs",
    "lrBandLimit", "lrWays", "centerWays", "subSpeakers", "scoopSpeakers",
    "cinemaLfeFilterBypass", "bassLinkMode", "bassFeed", "scoopFeed",
    "bassExtractor", "scoopExtractor",
    ENTROPY_CROSSOVER_16("gainDb"),
    ENTROPY_CROSSOVER_16("delayMs"),
    ENTROPY_CROSSOVER_16("processMode"),
    ENTROPY_CROSSOVER_16("firLength"),
    ENTROPY_CROSSOVER_16("hpOn"),
    ENTROPY_CROSSOVER_16("lpOn"),
    ENTROPY_CROSSOVER_16("hpHz"),
    ENTROPY_CROSSOVER_16("lpHz"),
    ENTROPY_CROSSOVER_16("filterType"),
    ENTROPY_CROSSOVER_16("allPass"),
    ENTROPY_CROSSOVER_16("apFreq"),
    ENTROPY_CROSSOVER_16("apQ"),
    ENTROPY_CROSSOVER_16("solo"),
    ENTROPY_CROSSOVER_16("source"),
    ENTROPY_CROSSOVER_16("cinemaRole"),
    ENTROPY_CROSSOVER_16("mute"),
    ENTROPY_CROSSOVER_16("emergencyMute"),
    ENTROPY_CROSSOVER_16("polarity"),
    ENTROPY_CROSSOVER_16("protectorOn"),
    ENTROPY_CROSSOVER_16("musicTrimDb"),
    ENTROPY_CROSSOVER_16("cinemaTrimDb"),
    ENTROPY_CROSSOVER_16("musicGainDb"),
    ENTROPY_CROSSOVER_16("cinemaGainDb"),
    "musicGlobalDelayMs", "cinemaGlobalDelayMs",
    ENTROPY_CROSSOVER_16("musicDelayMs"),
    ENTROPY_CROSSOVER_16("cinemaDelayMs"),
    ENTROPY_CROSSOVER_16("safetyMinHz"),
    ENTROPY_CROSSOVER_16("safetyMaxHz"),
    ENTROPY_CROSSOVER_16("tripHoldMs")
};

#undef ENTROPY_CROSSOVER_16

inline constexpr int parameterCount = sizeof(parameterIds) / sizeof(parameterIds[0]);

}
