#pragma once
#include "SourceDomain.h"
namespace entropy::music {
class InputMatrix {
  public:
    void process(const StereoBlock &, const StereoBlock &, StereoBlock &, uint32_t validity = 3) noexcept;
};
} // namespace entropy::music
