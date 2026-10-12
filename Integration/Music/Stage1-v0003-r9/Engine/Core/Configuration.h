#pragma once
#include "ProviderRegistry.h"
#include <filesystem>
namespace entropy::music {
struct SourceConfiguration {
    SourceKey key;
    uint32_t mode = 1;
};
struct Configuration {
    SourceConfiguration sources[2];
    std::filesystem::path path;
    static Configuration load(const std::filesystem::path &);
    bool save() const;
};
} // namespace entropy::music
