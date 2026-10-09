#include "DspAlignment.h"
#include <algorithm>
#include <limits>
namespace entropy::stage {
bool calculateAlignment(std::span<const PathLatency> modules,AlignmentProposal& result) noexcept {
 AlignmentProposal p;
 for(const auto& m:modules){if(!m.active)continue;if(!m.valid||!m.channels||(m.channels&~7u))return false;for(unsigned ch=0;ch<3;ch++)if(m.channels&(1u<<ch)){if(m.samples>UINT32_MAX-p.detected[ch])return false;p.detected[ch]+=m.samples;}}
 p.target=*std::max_element(p.detected.begin(),p.detected.end());for(unsigned ch=0;ch<3;ch++){p.delay[ch]=p.target-p.detected[ch];if(p.delay[ch]>65536)return false;}result=p;return true;
}
}
