#include "ProfileStore.h"
#include "DeviceService.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cwctype>
namespace entropy::control {
using J=ipc::Value;
namespace {
void requirePrivatePath(const std::filesystem::path& path){auto target=std::filesystem::weakly_canonical(path).wstring();auto stable=std::filesystem::weakly_canonical(std::filesystem::path(ENTROPY_STABLE_STAGE3_DIRECTORY)).wstring();for(auto& c:target)c=wchar_t(std::towlower(c));for(auto& c:stable)c=wchar_t(std::towlower(c));if(target==stable||(target.size()>stable.size()&&target.compare(0,stable.size(),stable)==0&&(target[stable.size()]=='\\'||target[stable.size()]=='/')))throw std::invalid_argument("Stable Stage3 profiles are protected; use a private copy");}
struct PersistedHeadroom {uint32_t magic=0x35524845,version=5;EntropyHeadroomState state{};uint32_t mode=1,reserved=0;double gain=-6;};
std::string checksum(const std::vector<unsigned char>& bytes){uint64_t c=14695981039346656037ULL;for(auto b:bytes)c=(c^b)*1099511628211ULL;std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<c;return s.str();}
std::string base64(const std::vector<unsigned char>& b){const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string s;for(size_t i=0;i<b.size();i+=3){unsigned n=unsigned(b[i])<<16;if(i+1<b.size())n|=unsigned(b[i+1])<<8;if(i+2<b.size())n|=b[i+2];s+=alphabet[(n>>18)&63];s+=alphabet[(n>>12)&63];s+=i+1<b.size()?alphabet[(n>>6)&63]:'=';s+=i+2<b.size()?alphabet[n&63]:'=';}return s;}
std::vector<unsigned char> decode(const std::string& s){if(s.size()>24*1024*1024||s.size()%4)throw std::invalid_argument("Invalid base64 length");std::vector<unsigned char> out;const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";for(size_t i=0;i<s.size();i+=4){unsigned n=0,pad=0;for(unsigned k=0;k<4;++k){auto c=s[i+k];if(c=='='){if(i+4!=s.size()||k<2)throw std::invalid_argument("Invalid base64 padding");++pad;n<<=6;}else{auto p=alphabet.find(c);if(p==std::string::npos||pad)throw std::invalid_argument("Invalid base64");n=(n<<6)|unsigned(p);}}out.push_back(uint8_t(n>>16));if(pad<2)out.push_back(uint8_t(n>>8));if(!pad)out.push_back(uint8_t(n));}if(base64(out)!=s)throw std::invalid_argument("Noncanonical base64");return out;}
template<class T>std::vector<unsigned char> bytes(const T& t){std::vector<unsigned char> b(sizeof(t));std::memcpy(b.data(),&t,sizeof(t));return b;}
}
void ProfileStore::save(const std::filesystem::path& path,ControlPlane& controls,const J& devices){
 requirePrivatePath(path);
 auto candidate=document_;if(candidate.isNull())candidate=J::Object{{"format","EntropyProject"},{"format_version",1},{"host_engine_rate",192000},{"modules",J::Array{J::Object{{"instance_id","crossover.main"}},J::Object{{"instance_id","matrix.output"}},J::Object{{"instance_id","headroom.main"}}}}};
 auto desired=controls.appliedState();PersistedHeadroom h;h.state=desired.headroom;h.mode=desired.mode;h.gain=desired.manualGainDb;auto hb=bytes(h),tb=bytes(desired.timing);hb.insert(hb.end(),tb.begin(),tb.end());
 std::array<std::vector<unsigned char>,3> payloads{controls.crossover().exportBytes(),bytes(desired.matrix),hb};std::array<bool,3> seen{};const char* moduleIds[]={"audio.entropy.crossover","audio.entropy.output-matrix.fp64-192","audio.entropy.headroom.manager.fp64"};
 for(auto& module:candidate["modules"].asArray()){auto id=module.at("instance_id").asString();int slot=id=="crossover.main"?0:id=="matrix.output"?1:id=="headroom.main"?2:-1;if(slot<0)continue;if(seen[slot])throw std::invalid_argument("Duplicate module");seen[slot]=true;module["module_id"]=moduleIds[slot];module["encoding"]="base64";module["serialized_state"]=base64(payloads[slot]);module["state_checksum_fnv1a64"]=checksum(payloads[slot]);}
 if(!seen[0]||!seen[1]||!seen[2])throw std::invalid_argument("Missing module");candidate["stage05_devices"]=devices;auto text=ipc::dump(candidate);
 auto temporary=path;temporary+=L".stage05.tmp";{std::ofstream f(temporary,std::ios::binary|std::ios::trunc);if(!f||!f.write(text.data(),text.size())||!f.flush())throw std::runtime_error("Cannot write profile");}
 if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit profile");document_=std::move(candidate);
}
LoadedProfile ProfileStore::load(const std::filesystem::path& path){
 requirePrivatePath(path);
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()>32*1024*1024)throw std::invalid_argument("Profile unavailable/oversized");auto size=f.tellg();std::string text(size_t(size),'\0');f.seekg(0);if(!f.read(text.data(),size))throw std::invalid_argument("Profile read failed");
 LoadedProfile p;p.document=ipc::parse(text);if(p.document.at("format").asString()!="EntropyProject"||p.document.at("format_version").asUnsigned()!=1||p.document.at("host_engine_rate").asUnsigned()!=192000)throw std::invalid_argument("Project contract incompatible");
 std::array<bool,3> seen{};for(const auto& module:p.document.at("modules").asArray()){auto id=module.at("instance_id").asString();int slot=id=="crossover.main"?0:id=="matrix.output"?1:id=="headroom.main"?2:-1;if(slot<0)continue;if(seen[slot])throw std::invalid_argument("Duplicate module");seen[slot]=true;const char* ids[]={"audio.entropy.crossover","audio.entropy.output-matrix.fp64-192","audio.entropy.headroom.manager.fp64"};if(module.at("module_id").asString()!=ids[slot]||module.at("encoding").asString()!="base64")throw std::invalid_argument("Module identity/encoding incompatible");auto b=decode(module.at("serialized_state").asString());if(checksum(b)!=module.at("state_checksum_fnv1a64").asString())throw std::invalid_argument("Profile checksum mismatch");
 if(slot==0)p.crossover=std::move(b);
 else if(slot==1){if(b.size()!=sizeof(p.parameters.matrix))throw std::invalid_argument("Matrix state size");std::memcpy(&p.parameters.matrix,b.data(),b.size());}
 else {if(b.size()!=sizeof(PersistedHeadroom)+sizeof(EntropyHeadroomTimingStateV5))throw std::invalid_argument("Headroom state version/size");PersistedHeadroom h;std::memcpy(&h,b.data(),sizeof(h));if(h.magic!=0x35524845||h.version!=5)throw std::invalid_argument("Headroom version");p.parameters.headroom=h.state;p.parameters.mode=h.mode;p.parameters.manualGainDb=h.gain;std::memcpy(&p.parameters.timing,b.data()+sizeof(h),sizeof(p.parameters.timing));}
 }
 if(!seen[0]||!seen[1]||!seen[2])throw std::invalid_argument("Missing physical module");
 // Validate all imported native states on a temporary offline graph before any live mutation.
 engine::AudioGraph graph;if(!graph.prepare(256))throw std::runtime_error("Profile validation prepare");p.parameters.renderLaneMask=0;if(p.document.contains("stage05_devices")){DeviceService devices;devices.restore(p.document.at("stage05_devices"));p.parameters.renderLaneMask=LiveSinkWorkers::renderLaneMask(LiveSinkWorkers::decode(devices.snapshot().at("configuration")));}
 ControlPlane candidate(graph);candidate.crossover().importBytes(p.crossover);candidate.replace(p.parameters);return p;
}
}
