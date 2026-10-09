#pragma once

#include "InputSourceCatalog.h"

#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host
{
enum class MusicRouteId : std::uint32_t
{
    privateTap = 1,
    entropyDirect = 2,
};

struct MusicRoute
{
    MusicRouteId id {};
    std::string title;
    InputSourceKey key;
    bool available = false;
    std::string detail;
};

bool productionMusicRoute(const InputSourceEntry& entry) noexcept;
std::vector<MusicRoute> buildMusicRoutes(const InputSourceSnapshot& snapshot);
}
