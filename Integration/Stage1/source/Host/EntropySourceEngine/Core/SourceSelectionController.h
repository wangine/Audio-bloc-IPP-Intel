#pragma once
#include "SourceDomainRuntime.h"
#include "SourceGraphRuntime.h"
#include "SourceStartupPolicy.h"
namespace entropy::host
{
class SourceSelectionController
{
  public:
    static bool needsRetry(bool prepared, const SourceDomainTelemetry &t) noexcept
    {
        return !prepared || t.capture_failed || t.state == 0;
    }
    SourceSelectionController(SourceDomainRuntime &domain, std::filesystem::path root)
        : domain_(domain), root_(std::move(root))
    {
    }
    void catalog(InputSourceSnapshot value) { catalog_ = std::move(value); }
    const InputSourceSnapshot &catalog() const noexcept { return catalog_; }
    bool selectMusic(const InputSourceKey &, std::string &error);
    const InputSourceKey &selected() const noexcept { return selected_; }
    uint64_t revision() const noexcept { return revision_; }
    uint32_t engine = 0, leftChannel = 0, rightChannel = 1;

  private:
    SourceDomainRuntime &domain_;
    std::filesystem::path root_;
    InputSourceSnapshot catalog_;
    InputSourceKey selected_{};
    uint64_t revision_ = 0;
};
} // namespace entropy::host
