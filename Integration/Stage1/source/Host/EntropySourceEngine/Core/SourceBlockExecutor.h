#pragma once
#include "SourceFrameDemand.h"
#include "AudioBlockReport.h"
#include "ProgramMixer.h"
#include "SourceGraphRuntime.h"
#include "StageBusValidation.h"
#include <array>
#include <entropy/input_matrix_abi.h>
namespace entropy::host
{
struct SourceExecutionResult
{
    bool passed = false;
    uint32_t fault = 0;
    StageBusBlockHeader header{};
    ProgramMixTelemetry mix{};
};
class SourceBlockExecutor
{
  public:
    bool prepare(SourceGraphRuntime &, std::string &error);
    SourceExecutionResult process(const entropy::stage::SourceFrameEnvelope &, uint32_t localMuteMask, const AudioBlockReport &,
                                  const AudioBlockReport &, const AudioBlockReport &,
                                  double *const *output10) noexcept;

  private:
    ModuleInstance *matrix_ = nullptr;
    EntropyInputMatrixApiV1 api_{};
    std::array<std::array<double, 1024>, 12> matrixOutput_{};
    std::array<double, 1024> silence_{};
};
} // namespace entropy::host
