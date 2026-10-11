#pragma once
#include <entropy/stage_bus_v1.h>
#include <cstdint>
namespace entropy::input {
struct DomainContext { StageBusSessionHeader session{}; StageBusBlockHeader block{}; uint64_t token=0; };
struct GraphInputContext { DomainContext music{},cinema{}; uint32_t domains=0; };
}
