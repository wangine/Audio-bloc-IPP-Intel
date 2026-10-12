#pragma once
#include "StageBusValidation.h"
namespace entropy::stage {
enum class DspExecuteResult {Ok,Invalid};
class TransparentDspExecutor {
public:
 DspExecuteResult process(const StageBusSessionHeader&,const StageBusSessionHeader&,const StageBusBlockHeader&,const double* const*,StageBusBlockHeader&,double* const*) noexcept;
};
}
