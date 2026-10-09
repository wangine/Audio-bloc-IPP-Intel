// Frozen baseline b928941; identifier renames only. Test oracle, never linked into release.
#pragma once
#include "AudioBlockReport.h"
#include "ProgramMixer.h"
#include "SourceGraphRuntime.h"
#include "StageBusValidation.h"
#include <array>
#include <entropy/input_matrix_abi.h>
namespace entropy::host
{
struct BaselineSourceExecutionResult
{
    bool passed = false;
    uint32_t fault = 0;
    StageBusBlockHeader header{};
    ProgramMixTelemetry mix{};
};
class BaselineSourceBlockExecutor
{
  public:
    bool prepare(SourceGraphRuntime &, std::string &error);
    BaselineSourceExecutionResult process(const StageBusFrameRequest &, const AudioBlockReport &,
                                  const AudioBlockReport &, const AudioBlockReport &,
                                  double *const *output10) noexcept;

  private:
    ModuleInstance *matrix_ = nullptr;
    EntropyInputMatrixApiV1 api_{};
    std::array<std::array<double, 1024>, 12> matrixOutput_{};
    std::array<double, 1024> silence_{};
};
} // namespace entropy::host
