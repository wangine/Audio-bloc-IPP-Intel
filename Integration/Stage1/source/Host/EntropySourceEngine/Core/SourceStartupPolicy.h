#pragma once
#include "InputSourceCatalog.h"
namespace entropy::host
{
struct SourceStartupPolicy
{
    static InputSourceKey musicKey();
    static bool permitted(const InputSourceKey &) noexcept;
};
} // namespace entropy::host
