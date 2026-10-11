#include "InputContract.h"
#include <StageBusValidation.h>
#include "StageLinkSecurity.h"
#include <cstring>
namespace entropy::input {
bool validateSessionForDomain(const StageBusSessionHeader& s,Domain domain) noexcept{
 const bool layout=domain==Domain::Music?s.layout_id==MUSIC_LRC3_V1:
  s.layout_id==CINEMA_51_6_V1||s.layout_id==CINEMA_71_8_V1;
 return layout&&stage::validateSession(s,s.mapping_bytes)==stage::StageBusValidationResult::Ok;
}
bool validateBlockForDomain(const StageBusSessionHeader& s,const StageBusBlockHeader& b,Domain domain) noexcept{
 if(!validateSessionForDomain(s,domain)||std::memcmp(s.session_id,b.session_id,16)||!b.stream_epoch||
  !b.block_sequence||!b.control_revision||b.frames!=1024||b.reserved||b.first_frame>UINT64_MAX-1024)return false;
 const unsigned mask=domain==Domain::Music?3u:4u;
 if((b.validity_mask|b.discontinuity_mask)&~mask)return false;
 if(domain==Domain::Music)return !b.source_generation[2]&&!b.source_state[2]&&!b.source_rate[2];
 return !b.music_session_revision&&!b.source_generation[0]&&!b.source_generation[1]&&
  !b.source_state[0]&&!b.source_state[1]&&!b.source_rate[0]&&!b.source_rate[1];
}
std::wstring defaultEndpoint(Domain domain){
 auto endpoint=link::endpointName();const auto at=endpoint.find(L"StageLink.V1");
 if(at==std::wstring::npos)return {};
 endpoint.replace(at,12,domain==Domain::Music?L"StagePublish.Music.Dsp.V2":L"StagePublish.Cinema.Decoder.V2");
 return endpoint;
}
bool localEndpoint(const std::wstring& endpoint) noexcept{
 const std::wstring_view prefix=L"\\\\.\\pipe\\";
 if(endpoint.size()<=prefix.size()||endpoint.size()>240||endpoint.compare(0,prefix.size(),prefix))return false;
 for(size_t i=prefix.size();i<endpoint.size();++i){const auto c=endpoint[i];
  if(!((c>=L'A'&&c<=L'Z')||(c>=L'a'&&c<=L'z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-'))return false;
 }
 return true;
}
}
