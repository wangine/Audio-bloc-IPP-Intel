#include "TestSupport.h"
int main(){
 auto stereo=session(3),lrc=makeSession(4,stereo.session_id,stereo.qpc_frequency);
 CHECK(stereo.channels==2 && lrc.channels==3);
 CHECK(stereo.slot_stride==16512 && lrc.slot_stride==24704);
 CHECK(validateSession(stereo,stereo.mapping_bytes)==StageBusValidationResult::Ok);
 CHECK(validateSession(lrc,lrc.mapping_bytes)==StageBusValidationResult::Ok);
 for(auto original:{stereo,lrc}){
  auto bad=original;++bad.channels;CHECK(validateSession(bad,bad.mapping_bytes)!=StageBusValidationResult::Ok);
  bad=original;++bad.slot_stride;CHECK(validateSession(bad,bad.mapping_bytes)!=StageBusValidationResult::Ok);
  bad=original;bad.layout_hash[0]^=1;CHECK(validateSession(bad,bad.mapping_bytes)!=StageBusValidationResult::Ok);
  bad=original;bad.contract_hash[0]^=1;CHECK(validateSession(bad,bad.mapping_bytes)!=StageBusValidationResult::Ok);
  CHECK(validateSession(original,original.mapping_bytes-1)!=StageBusValidationResult::Ok);
  auto b=header(request(original,71));b.validity_mask=3;b.discontinuity_mask=1;b.source_generation[2]=0;
  CHECK(validateBlock(original,b,original.slot_stride)==StageBusValidationResult::Ok);
  b.validity_mask|=4;CHECK(validateBlock(original,b,original.slot_stride)!=StageBusValidationResult::Ok);
  b.validity_mask=3;b.source_generation[2]=13;CHECK(validateBlock(original,b,original.slot_stride)!=StageBusValidationResult::Ok);
 }
 CHECK(session(MIX_CINEMA10_V1).channels==10 && session(LRC_CINEMA11_V1).channels==11);
 CHECK(validateSession(session(99),100000)==StageBusValidationResult::Incompatible);
 std::cout<<"PASS MUSIC 2/3 layout identity, bounds, hash, no CINEMA metadata, legacy identity\n";
}
