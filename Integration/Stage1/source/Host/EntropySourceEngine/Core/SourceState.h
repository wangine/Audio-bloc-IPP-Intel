#pragma once
#include "SourceStartupPolicy.h"
#include <QJsonObject>
#include <array>
namespace entropy::host
{
struct SourceStateSnapshot
{
    uint32_t manualMuteMask = 0;
    std::array<uint32_t, 3> engines{};
    uint32_t leftChannel = 0, rightChannel = 1;
    uint64_t revision = 1;
    InputSourceKey current = SourceStartupPolicy::musicKey();
};
class SourceState
{
  public:
    explicit SourceState(std::filesystem::path root) : root_(std::move(root)) {}
    bool importR10(const std::filesystem::path &, const std::filesystem::path &, std::string &);
    bool save(const SourceStateSnapshot &, std::string &);
    bool load(SourceStateSnapshot &, std::string &);
    const QJsonObject &preserved() const noexcept { return opaque_; }

  private:
    std::filesystem::path root_;
    QJsonObject opaque_;
};
} // namespace entropy::host
