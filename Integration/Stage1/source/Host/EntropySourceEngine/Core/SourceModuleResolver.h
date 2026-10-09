#pragma once
#include "ModuleRuntime.h"
namespace entropy::host
{
class SourceModuleResolver
{
  public:
    virtual ~SourceModuleResolver() = default;
    virtual ModuleInstance *findInstance(const char *) const noexcept = 0;
    virtual bool readyStopped() const noexcept = 0;
};
} // namespace entropy::host
