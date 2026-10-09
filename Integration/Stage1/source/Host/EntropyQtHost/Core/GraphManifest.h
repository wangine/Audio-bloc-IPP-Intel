#pragma once

#include "ModuleRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace entropy::host
{
struct ManifestModule
{
    const char* identifier;
    std::uint32_t kind;
    const char* role;
    bool requiredForProductionGraph;
};

struct ManifestCheck
{
    ManifestModule expected{};
    const ModuleRecord* module = nullptr;
    bool valid = false;
    std::string detail;
};

struct ManifestReport
{
    std::vector<ManifestCheck> checks;
    bool productionGraphReady = false;
    std::size_t requiredFound = 0;
    std::size_t requiredTotal = 0;
    std::size_t canonicalFound = 0;
};

class GraphManifest
{
public:
    static const std::vector<ManifestModule>& canonicalModules();
    static ManifestReport validate(const ModuleRegistry& registry);
};
} // namespace entropy::host
