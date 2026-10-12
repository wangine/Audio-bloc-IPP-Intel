#include "TransparentDspExecutor.h"
#include <cstring>
namespace entropy::stage {
DspExecuteResult TransparentDspExecutor::process(const StageBusSessionHeader& src,const StageBusSessionHeader& dst,const StageBusBlockHeader& b,const double* const* in,StageBusBlockHeader& result,double* const* out) noexcept {
 if(src.layout_id!=MUSIC_STEREO2_V1 || dst.layout_id!=MUSIC_LRC3_V1 ||
    validateSession(src,src.mapping_bytes)!=StageBusValidationResult::Ok ||
    validateSession(dst,dst.mapping_bytes)!=StageBusValidationResult::Ok ||
    std::memcmp(src.session_id,dst.session_id,16) || src.qpc_frequency!=dst.qpc_frequency ||
    validateBlock(src,b,src.slot_stride)!=StageBusValidationResult::Ok || b.stream_epoch!=1 || !in || !out)
  return DspExecuteResult::Invalid;
 for(unsigned c=0;c<2;c++)if(!in[c])return DspExecuteResult::Invalid;
 for(unsigned c=0;c<3;c++)if(!out[c])return DspExecuteResult::Invalid;
 for(unsigned c=0;c<2;c++)std::memcpy(out[c],in[c],8192);
 std::memset(out[2],0,8192);result=b;return DspExecuteResult::Ok;
}
}
