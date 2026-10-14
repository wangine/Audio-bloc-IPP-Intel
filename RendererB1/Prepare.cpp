// OFFLINE ONLY: validate saved Stage2 PCM, apply the existing profile/DSP,
// quantize before playback, write immutable per-DAC files. No source/DAC is opened.
#include "CoreFile.h"
#include "Engine/Control/ProfileStore.h"
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <entropy/stage_bus_v1.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <array>
#include <cmath>
#include <limits>
using namespace entropy;
namespace {
struct Record {uint64_t receivedQpc;StageBusBlockHeader header;double samples[3][1024];};
static_assert(sizeof(Record)==24712);static_assert(sizeof(double)==8);
struct Output {
 bool enabled=false;b1::Header header;std::ofstream file;
 std::unique_ptr<pcm24::Pcm24ModuleEngine> converter;
 std::vector<uint8_t> packet;std::array<const double*,16> inputs{};
};
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
uint32_t channelMask(unsigned c){switch(c){case 1:return 4;case 2:return 3;case 4:return 0x33;case 6:return 0x60f;case 8:return 0x63f;default:return 0;}}
std::string name(unsigned slot){return slot?"dac-b.pcm":"dac-a.pcm";}
}
int wmain(int argc,wchar_t** argv) try {
 require(argc==4,"Usage: EntropyPrepareB1 PROFILE.entropy STAGE2_CAPTURE.bin NEW_OUTPUT_DIRECTORY");
 const std::filesystem::path profile=argv[1],capture=argv[2],folder=argv[3];
 require(!std::filesystem::exists(folder),"Output directory exists; never overwrite");
 std::ifstream inputFile(capture,std::ios::binary|std::ios::ate);require(bool(inputFile),"Capture unavailable");
 const auto fileSize=inputFile.tellg();require(fileSize>std::streamoff(sizeof(StageBusSessionHeader)),"Empty capture");
 const auto payloadSize=uint64_t(fileSize)-sizeof(StageBusSessionHeader);
 require(payloadSize%sizeof(Record)==0,"Partial capture record");const auto count=payloadSize/sizeof(Record);
 require(count>0&&count<=45000,"Capture length must be bounded to 240 seconds");
 StageBusSessionHeader session{};inputFile.seekg(0);require(bool(inputFile.read(reinterpret_cast<char*>(&session),sizeof(session))),"Session read");
 require(session.magic==STAGE_BUS_MAGIC&&session.abi_version==STAGE_BUS_ABI&&session.header_bytes==256&&
  session.layout_id==MUSIC_LRC3_V1&&session.channels==3&&session.sample_rate==192000&&
  session.sample_format==STAGE_BUS_BINARY64_LE&&session.planar==STAGE_BUS_PLANAR&&session.frames_per_block==1024&&
  session.block_header_bytes==128&&session.plane_stride==8192&&session.qpc_frequency>0,"Capture is not supported Stage2 MUSIC L/R/CX FP64/192");
 // ProfileStore validates native payloads using the unchanged offline control code.
 control::ProfileStore profiles;auto loaded=profiles.load(profile);
 require(loaded.parameters.mode==ENTROPY_HEADROOM_MODE_BIT_PERFECT||loaded.parameters.mode==ENTROPY_HEADROOM_MODE_MANUAL_SAFE,
  "B1 preserves MANUAL/BITPERFECT; an AUTO profile is refused, not silently changed");
 const auto domains=control::LiveSinkWorkers::decode(loaded.document.at("stage05_devices").at("configuration"));
 uint32_t mask=0;std::array<bool,16> used{};unsigned enabled=0;
 for(unsigned slot=0;slot<2;++slot){const auto& d=domains[slot];if(!d.enabled)continue;++enabled;
  require(d.channels>0&&d.channels<=16&&!d.endpoint.empty()&&d.logicalId==(slot?"DAC_B":"DAC_A"),"Invalid saved DAC identity/channels");
  if(slot&&domains[0].enabled)require(d.endpoint!=domains[0].endpoint,"Duplicate endpoint");
  for(unsigned c=0;c<d.channels;++c){require(d.lanes[c]<16&&!used[d.lanes[c]],"Invalid/duplicate saved lane");used[d.lanes[c]]=true;mask|=1u<<d.lanes[c];}
 }
 require(enabled>0,"No configured DAC");
 auto graph=std::make_unique<engine::AudioGraph>();require(graph->prepare(1024),"Graph preparation failed");graph->enableOriginalMusicAnalysis(true);
 control::ControlPlane controls(*graph);controls.setRenderLaneMask(mask);controls.atBlockBoundary();controls.afterBlock();
 loaded.parameters.renderLaneMask=mask;controls.crossover().importBytes(loaded.crossover);controls.replace(loaded.parameters);controls.atBlockBoundary();controls.afterBlock();
 require(controls.crossover().snapshot().at("program").asUnsigned(1)==0,"Saved profile selects CINEMA; B1 input is a MUSIC capture. No automatic switch.");
 std::filesystem::create_directories(folder);std::ofstream incomplete(folder/"INCOMPLETE.txt");incomplete<<"Offline preparation incomplete. Do not play.\n";incomplete.close();
 engine::PlanarBuffer in,out;in.prepare(13,1024);out.prepare(16,1024);in.clear(1024);
 std::array<Output,2> outputs;
 for(unsigned slot=0;slot<2;++slot){auto& o=outputs[slot];const auto& d=domains[slot];o.enabled=d.enabled;if(!o.enabled)continue;
  o.header.channels=d.channels;o.header.containerBits=d.format?32:24;o.header.channelMask=channelMask(d.channels);
  o.packet.resize(size_t(1024)*d.channels*(o.header.containerBits/8));
  EntropyPcm24Config p{};p.struct_size=sizeof(p);p.revision=1;p.channels=d.channels;p.maximum_frames=1024;p.output_format=d.format;
  p.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;p.seed_mode=ENTROPY_PCM24_SEED_DETERMINISTIC_TEST;
  p.seed=UINT64_C(0x423150434d323400)+slot;p.reserved_u32=ENTROPY_PCM24_OVERRANGE_FAIL_CLOSED;
  o.converter=std::make_unique<pcm24::Pcm24ModuleEngine>();require(o.converter->configure(p),"PCM24 configure failed");
  o.file.open(folder/name(slot),std::ios::binary);require(bool(o.file.write(reinterpret_cast<const char*>(&o.header),sizeof(o.header))),"PCM header write failed");
 }
 auto record=std::make_unique<Record>();StageBusBlockHeader first{},previous{};uint64_t rawHash=b1::hashSeed,mappedHash=b1::hashSeed;double maximum=0;
 const auto process=[&]{
  controls.atBlockBoundary();engine::GraphControlAccess::crossover(*graph)->setProgramInputEpochs(1,1,0,0xffffu);
  require(graph->process(in.readPointers(),out.writePointers(),1024)==ENTROPY_OK,"Offline graph processing failed");controls.afterBlock();
  for(unsigned slot=0;slot<2;++slot){auto& o=outputs[slot];const auto& d=domains[slot];if(!o.enabled)continue;
   for(unsigned c=0;c<d.channels;++c){o.inputs[c]=out.readChannel(d.lanes[c]);for(unsigned f=0;f<1024;++f){const double v=o.inputs[c][f];require(std::isfinite(v),"Nonfinite graph output");maximum=std::max(maximum,std::abs(v));}}
   EntropyPcm24BlockF64 p{};p.struct_size=sizeof(p);p.channels=d.channels;p.frames=1024;p.inputs=o.inputs.data();p.output_interleaved=o.packet.data();p.output_capacity_bytes=unsigned(o.packet.size());
   require(o.converter->process(p)==ENTROPY_OK&&p.produced_bytes==o.packet.size(),"PCM24 rejected graph output (including overrange); no extra attenuation applied");
   require(bool(o.file.write(reinterpret_cast<const char*>(o.packet.data()),std::streamsize(o.packet.size()))),"PCM payload write failed");
   o.header.payloadHash=b1::hash(o.packet,o.header.payloadHash);o.header.frames+=1024;
  }
 };
 for(uint64_t index=0;index<count;++index){
  require(bool(inputFile.read(reinterpret_cast<char*>(record.get()),sizeof(*record))),"Capture read failed");const auto& h=record->header;
  require(std::memcmp(h.session_id,session.session_id,16)==0&&h.frames==1024&&h.stream_epoch&&h.block_sequence&&h.control_revision&&!h.reserved,
   "Invalid block identity/size");
  require(h.first_frame<=UINT64_MAX-1024&&((h.validity_mask|h.discontinuity_mask)&~3u)==0,"Invalid MUSIC block masks/frame origin");
  if(index){require(h.stream_epoch==previous.stream_epoch&&previous.block_sequence!=UINT64_MAX&&h.block_sequence==previous.block_sequence+1&&h.first_frame==previous.first_frame+1024,
    "Saved capture contains a timeline gap; no interpolation/repair allowed");
   require(h.music_session_revision==first.music_session_revision&&h.source_generation[0]==first.source_generation[0]&&h.source_generation[1]==first.source_generation[1],"Capture changes source generation; isolate one saved session");
  }else first=h;
  previous=h;
  for(unsigned c=0;c<3;++c){for(double v:record->samples[c])require(std::isfinite(v),"Nonfinite captured PCM");
   const auto* src=reinterpret_cast<const uint8_t*>(record->samples[c]);rawHash=b1::hash({src,8192},rawHash);
   std::memcpy(in.writePointers()[c],record->samples[c],8192);
   const auto* dst=reinterpret_cast<const uint8_t*>(in.readPointers()[c]);require(std::memcmp(src,dst,8192)==0,"Offline input mapping changed PCM");mappedHash=b1::hash({dst,8192},mappedHash);
  }
  process();
 }
 // Explicit EOS only, not underrun concealment. One second rounded up to blocks.
 constexpr unsigned tailBlocks=188;in.clear(1024);for(unsigned n=0;n<tailBlocks;++n)process();
 ipc::Value::Array files;
 for(unsigned slot=0;slot<2;++slot){auto& o=outputs[slot];const auto& d=domains[slot];if(!o.enabled)continue;
  (void)b1::payloadBytes(o.header);o.file.seekp(0);require(bool(o.file.write(reinterpret_cast<const char*>(&o.header),sizeof(o.header))),"Final header write failed");o.file.flush();require(bool(o.file),"PCM flush failed");o.file.close();require(!o.file.fail(),"PCM close failed");
  require(std::filesystem::file_size(folder/name(slot))==sizeof(b1::Header)+b1::payloadBytes(o.header),"Prepared file size mismatch");
  ipc::Value::Array lanes;for(unsigned c=0;c<d.channels;++c)lanes.emplace_back(d.lanes[c]);
  files.emplace_back(ipc::Value::Object{{"dac",slot?"B":"A"},{"file",name(slot)},{"endpoint",d.endpoint},{"channels",d.channels},{"containerBits",o.header.containerBits},{"rate",192000},{"frames",ipc::Value::u64(o.header.frames)},{"lanes",std::move(lanes)}});
 }
 require(rawHash==mappedHash,"Input hash disagreement");
 auto manifest=ipc::Value::Object{{"schema","entropy-renderer-b1-v1"},{"sourceBlocks",ipc::Value::u64(count)},{"sourceFirstSequence",ipc::Value::u64(first.block_sequence)},{"sourceLastSequence",ipc::Value::u64(previous.block_sequence)},{"sourceFirstFrame",ipc::Value::u64(first.first_frame)},{"sourceFrames",ipc::Value::u64(count*1024)},{"tailFrames",tailBlocks*1024},{"inputCopyExact",true},{"inputFNV1a64",ipc::Value::u64(rawHash)},{"headroomMode",loaded.parameters.mode},{"manualGainDb",loaded.parameters.manualGainDb},{"maximumPrePcmMagnitude",maximum},{"deterministicTpdf",true},{"liveInputVerified",false},{"audioQualityVerified",false},{"outputs",std::move(files)}};
 const auto text=ipc::dump(ipc::Value(std::move(manifest)));std::ofstream m(folder/"prepared.json",std::ios::binary);require(bool(m.write(text.data(),std::streamsize(text.size()))),"Manifest write failed");m.flush();require(bool(m),"Manifest flush failed");m.close();require(!m.fail(),"Manifest close failed");
 std::filesystem::remove(folder/"INCOMPLETE.txt");std::cout<<"PREPARED offline: "<<count<<" captured blocks; input copy exact; original DSP and PCM24 used. No DAC opened.\n";return 0;
}catch(const std::exception& e){std::cerr<<"B1 PREPARE STOP: "<<e.what()<<'\n';return 1;}
