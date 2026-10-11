#include <StageBusValidation.h>
#include "Engine/Input/InputContract.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace entropy::stage;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main() try {
 const uint8_t uuid[16]={9,8,7,6,5,4,3,2,1,10,11,12,13,14,15,16};
 const uint8_t musicHash[32]={141,29,208,208,183,51,72,154,131,235,181,168,78,210,135,173,33,17,117,210,125,127,190,166,79,169,105,19,15,85,49,142};
 const uint8_t lrcHash[32]={215,107,25,74,200,88,229,189,219,142,80,174,196,164,44,8,254,253,173,67,41,19,178,176,160,244,145,168,53,66,178,63};
 auto music=makeSession(4,uuid,10000000);
 check(music.channels==3,"MUSIC_LRC3 must declare exactly three planes");
 check(music.slot_stride==24704&&music.mapping_bytes==102912,"MUSIC session bounds must match Stage2 v0009");
 check(!std::memcmp(music.contract_hash,musicHash,32)&&!std::memcmp(music.layout_hash,lrcHash,32),"MUSIC must preserve distributed Stage2 hashes");
 check(validateSession(music,102912)==StageBusValidationResult::Ok,"real MUSIC session accepted");
 check(entropy::input::validateSessionForDomain(music,entropy::input::Domain::Music),"typed MUSIC input accepts distributed MUSIC3");
 check(!entropy::input::validateSessionForDomain(music,entropy::input::Domain::Cinema),"Cinema input refuses MUSIC session");
 for(unsigned layout:{1u,2u,3u}){
  const auto legacy=makeSession(layout,uuid,10000000);
  check(!entropy::input::validateSessionForDomain(legacy,entropy::input::Domain::Music),"production MUSIC refuses legacy/stereo contracts");
  check(!entropy::input::validateSessionForDomain(legacy,entropy::input::Domain::Cinema),"production CINEMA refuses legacy/stereo contracts");
 }
 for(unsigned layout:{5u,6u}){
  auto cinema=makeSession(layout,uuid,10000000);
  check(cinema.channels==(layout==5?6u:8u),"CINEMA layout must declare six or eight planes");
  check(validateSession(cinema,cinema.mapping_bytes)==StageBusValidationResult::Ok,"CINEMA session accepted");
  check(std::memcmp(cinema.contract_hash,musicHash,32)!=0,"CINEMA and MUSIC have distinct contracts");
  check(entropy::input::validateSessionForDomain(cinema,entropy::input::Domain::Cinema),"typed Cinema accepts 6/8ch");
  check(!entropy::input::validateSessionForDomain(cinema,entropy::input::Domain::Music),"Music input refuses Cinema session");
  StageBusBlockHeader block{};std::memcpy(block.session_id,uuid,16);block.stream_epoch=17;block.block_sequence=123;
  block.control_revision=11;block.frames=1024;block.first_frame=987600;block.validity_mask=4;block.source_generation[2]=20;block.source_rate[2]=48000;
  check(entropy::input::validateBlockForDomain(cinema,block,entropy::input::Domain::Cinema),"Cinema preserves arbitrary real first frame origin");
  block.source_generation[0]=1;
  check(!entropy::input::validateBlockForDomain(cinema,block,entropy::input::Domain::Cinema),"Cinema refuses mixed Music metadata");
  block.source_generation[0]=0;block.validity_mask=1;
  check(!entropy::input::validateBlockForDomain(cinema,block,entropy::input::Domain::Cinema),"Cinema refuses Music validity mask");
 }
 for(unsigned mutation=0;mutation<12;++mutation){
  auto bad=music;
  switch(mutation){case 0:bad.channels=11;break;case 1:bad.sample_rate=48000;break;case 2:bad.sample_format=2;break;case 3:bad.planar=0;break;case 4:bad.frames_per_block=512;break;case 5:bad.plane_stride=16384;break;case 6:bad.slot_stride++;break;case 7:bad.mapping_bytes--;break;case 8:bad.contract_hash[0]^=1;break;case 9:bad.layout_hash[0]^=1;break;case 10:std::memset(bad.session_id,0,16);break;case 11:bad.reserved[0]=1;break;}
  check(validateSession(bad,bad.mapping_bytes)!=StageBusValidationResult::Ok,"malformed session rejected before payload");
 }
 check(validateSession(music,102911)!=StageBusValidationResult::Ok,"short mapping rejected");
 StageBusBlockHeader block{};std::memcpy(block.session_id,uuid,16);block.stream_epoch=17;block.block_sequence=123;block.control_revision=11;block.frames=1024;block.first_frame=987600;block.validity_mask=3;
 check(entropy::input::validateBlockForDomain(music,block,entropy::input::Domain::Music),"Music block preserves real timeline");
 block.source_rate[2]=48000;
 check(!entropy::input::validateBlockForDomain(music,block,entropy::input::Domain::Music),"Music input refuses Cinema metadata");
 check(entropy::input::localEndpoint(L"\\\\.\\pipe\\Entropy.Test.Local"),"local test endpoint accepted");
 check(!entropy::input::localEndpoint(L"\\\\server\\pipe\\Entropy.Test"),"remote endpoint refused");
 check(entropy::input::defaultEndpoint(entropy::input::Domain::Music).find(L"Entropy.StagePublish.Music.Dsp.V2.")!=std::wstring::npos,"default Music endpoint matches Stage2");
 check(entropy::input::defaultEndpoint(entropy::input::Domain::Cinema).find(L"Entropy.StagePublish.Cinema.Decoder.V2.")!=std::wstring::npos,"default Cinema endpoint distinct");
 std::cout<<"PASS distributed MUSIC3, CINEMA6/8, malformed sessions and mapping bounds\n";
 return 0;
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
