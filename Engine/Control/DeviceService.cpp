#include "DeviceService.h"
#include <objbase.h>
#include <set>
#include <cstring>
namespace entropy::control {
using J=ipc::Value;
namespace {
template<size_t N>void stringField(char (&dst)[N],const J& value){auto& text=value.asString();if(text.size()>=N||text.find(char(0))!=std::string::npos)throw std::invalid_argument("Invalid device string");std::memcpy(dst,text.c_str(),text.size()+1);}
template<class T>J facts(const T& d){J::Array evidence,formats;for(unsigned i=0;i<d.evidence_count;++i){const auto& e=d.evidence[i];evidence.emplace_back(J::Object{{"kind",e.kind},{"stability",e.stability},{"flags",e.flags},{"value",e.value_utf8}});}for(unsigned i=0;i<d.format_count;++i){const auto& f=d.formats[i];formats.emplace_back(J::Object{{"rate",f.sample_rate},{"channels",f.channels},{"containerBits",f.container_bits},{"validBits",f.valid_bits},{"encoding",f.encoding}});}return J::Object{{"evidence",std::move(evidence)},{"formats",std::move(formats)}};}
void validateDomains(const J::Array& domains){if(domains.size()>2)throw std::invalid_argument("At most DAC A/B");std::set<std::string> endpoints;std::set<unsigned> lanes;for(size_t slot=0;slot<domains.size();++slot){const auto& d=domains[slot];if(!d.at("enabled").asBool())continue;if(d.at("logicalId").asString()!=(slot?"DAC_B":"DAC_A")||!d.at("approvalRevision").asU64())throw std::invalid_argument("Invalid DAC binding request");if(!endpoints.insert(d.at("endpoint").asString()).second)throw std::invalid_argument("Duplicate DAC endpoint");auto channels=d.at("channels").asUnsigned(16);if(d.at("rate").asUnsigned()!=192000||!channels||d.at("format").asUnsigned(1)>1||d.at("lanes").asArray().size()!=channels)throw std::invalid_argument("Invalid DAC format/lanes");for(const auto& lane:d.at("lanes").asArray())if(!lanes.insert(lane.asUnsigned(15)).second)throw std::invalid_argument("Duplicate physical output lane");}}
}

J DeviceService::refresh(){auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto r=inventory_.refresh();if(SUCCEEDED(com))CoUninitialize();if(r!=ENTROPY_OK)throw std::runtime_error("Native device inventory unavailable");evidenceGeneration_=0;return snapshot();}
J DeviceService::revalidateSelected(){
 changes_.ensure();std::vector<std::string> selected;
 for(const auto& domain:config_.asArray())if(domain.at("enabled").asBool())selected.push_back(domain.at("endpoint").asString());
 if(selected.empty())throw std::invalid_argument("No enabled DAC configured");
 const auto generation=changes_.generation();
 if(generation!=evidenceGeneration_||selected!=validatedEndpoints_){
  if(inventory_.refreshSelected(selected)!=ENTROPY_OK)throw std::runtime_error("Selected device inventory unavailable");
  if(generation!=changes_.generation())throw std::runtime_error("Device changed during validation; retry explicit ARM");
  evidenceGeneration_=generation;validatedEndpoints_=selected;
 }
 // Even a warm evidence cache checks the actual selected endpoints at ARM.
 if(!changes_.selectedActive(selected)){evidenceGeneration_=0;throw std::invalid_argument("Selected DAC offline");}
 if(generation!=changes_.generation()){evidenceGeneration_=0;throw std::runtime_error("Selected DAC changed during validation");}
 return configure(J::Object{{"domains",config_}});
}
J DeviceService::snapshot() const {
 J::Array list;for(const auto& d:inventory_.devices()){J::Array formats,evidence;
 for(unsigned i=0;i<d.format_count;++i){auto& f=d.formats[i];formats.emplace_back(J::Object{{"rate",f.sample_rate},{"channels",f.channels},{"containerBits",f.container_bits},{"validBits",f.valid_bits},{"encoding",f.encoding}});}
 for(unsigned i=0;i<d.evidence_count;++i){auto& e=d.evidence[i];evidence.emplace_back(J::Object{{"kind",e.kind},{"stability",e.stability},{"flags",e.flags},{"value",e.value_utf8}});}
 list.emplace_back(J::Object{{"endpoint",d.endpoint_id_utf8},{"name",d.display_name_utf8},{"fingerprint",d.composite_fingerprint},{"active",bool(d.active)},{"default",bool(d.is_default)},{"maximumChannels",d.maximum_channels},{"formats",std::move(formats)},{"evidence",std::move(evidence)}});}
 J::Array bindings;for(unsigned i=0;i<2;++i){if(!present_[i]){bindings.emplace_back(nullptr);continue;}const auto& b=bindings_[i];auto resolution=device::resolveBinding(b,inventory_.devices());auto record=facts(b);record["logicalId"]=b.logical_device_id;record["endpoint"]=b.approved_endpoint_id_utf8;record["name"]=b.approved_display_name_utf8;record["fingerprint"]=b.composite_fingerprint;record["approvalRevision"]=J::u64(b.approval_revision);record["state"]=resolution.state;record["resolvedEndpoint"]=resolution.resolved_endpoint_id_utf8;record["detail"]=resolution.detail_utf8;bindings.emplace_back(std::move(record));}
 return J::Object{{"devices",std::move(list)},{"configuration",config_},{"bindings",std::move(bindings)}};
}
J DeviceService::bindingCommand(const J& request){auto slot=request.at("slot").asUnsigned(1);auto action=request.at("action").asString();if(action=="clear-binding"){bindings_[slot]={};present_[slot]=false;if(slot<config_.asArray().size())config_.asArray()[slot]["enabled"]=false;return snapshot();}auto endpoint=request.at("endpoint").asString();const EntropyDeviceInfo* selected=nullptr;for(const auto& d:inventory_.devices())if(endpoint==d.endpoint_id_utf8&&d.active)selected=&d;if(!selected)throw std::invalid_argument("Endpoint offline");for(unsigned i=0;i<2;++i)if(i!=slot&&present_[i]&&endpoint==bindings_[i].approved_endpoint_id_utf8)throw std::invalid_argument("Physical DAC already assigned");std::string logical=slot?"DAC_B":"DAC_A";auto revision=request.at("approvalRevision").asU64();if(!revision)throw std::invalid_argument("Approval revision required");bindings_[slot]=device::captureBinding(*selected,logical.c_str(),revision);present_[slot]=true;return snapshot();}
J DeviceService::configure(const J& request){
 const auto& domains=request.at("domains").asArray();validateDomains(domains);
 for(size_t slot=0;slot<domains.size();++slot){const auto& d=domains[slot];if(!d.at("enabled").asBool())continue;
  if(!present_[slot]||d.at("approvalRevision").asU64()!=bindings_[slot].approval_revision)throw std::invalid_argument("Explicit current DAC approval required");
  auto resolution=device::resolveBinding(bindings_[slot],inventory_.devices());auto endpoint=d.at("endpoint").asString();
  if(resolution.state!=ENTROPY_DEVICE_VERIFIED||endpoint!=resolution.resolved_endpoint_id_utf8)throw std::invalid_argument("Native binding not verified");
  bool exact=false;for(const auto& found:inventory_.devices())if(endpoint==found.endpoint_id_utf8&&found.active)for(unsigned i=0;i<found.format_count;++i){const auto& f=found.formats[i];if(f.sample_rate==d.at("rate").asUnsigned()&&f.channels==d.at("channels").asUnsigned()&&f.valid_bits==24&&f.container_bits==(d.at("format").asUnsigned()?32u:24u)&&f.encoding==ENTROPY_DEVICE_PCM_INTEGER)exact=true;}
  if(!exact)throw std::invalid_argument("Driver does not accept exact PCM24 format");
 }
 config_=domains;return snapshot();
}
void DeviceService::restore(const J& value){
 const auto& records=value.at("bindings").asArray();if(records.size()!=2)throw std::invalid_argument("Invalid DAC binding count");
 std::array<EntropyDeviceBinding,2> candidate{};std::array<bool,2> present{};std::set<std::string> endpoints;
 for(unsigned slot=0;slot<2;++slot){const auto& r=records[slot];if(r.isNull())continue;auto& b=candidate[slot];b.struct_size=sizeof(b);b.schema_version=ENTROPY_DEVICE_BINDING_SCHEMA_VERSION;
  if(r.at("logicalId").asString()!=(slot?"DAC_B":"DAC_A"))throw std::invalid_argument("DAC logical identity mismatch");stringField(b.logical_device_id,r.at("logicalId"));stringField(b.approved_endpoint_id_utf8,r.at("endpoint"));stringField(b.approved_display_name_utf8,r.at("name"));stringField(b.composite_fingerprint,r.at("fingerprint"));b.approval_revision=r.at("approvalRevision").asU64();if(!b.approval_revision||!b.approved_endpoint_id_utf8[0]||!endpoints.insert(b.approved_endpoint_id_utf8).second)throw std::invalid_argument("Invalid/duplicate approval");
  auto& evidence=r.at("evidence").asArray();auto& formats=r.at("formats").asArray();if(evidence.size()>ENTROPY_DEVICE_MAX_EVIDENCE||formats.size()>ENTROPY_DEVICE_MAX_FORMATS)throw std::invalid_argument("Device evidence capacity");b.evidence_count=unsigned(evidence.size());b.format_count=unsigned(formats.size());
  for(unsigned i=0;i<b.evidence_count;++i){auto& e=b.evidence[i];const auto& j=evidence[i];e.struct_size=sizeof(e);e.kind=j.at("kind").asUnsigned(11);e.stability=j.at("stability").asUnsigned(5);e.flags=j.at("flags").asUnsigned(15);stringField(e.value_utf8,j.at("value"));}
  for(unsigned i=0;i<b.format_count;++i){auto& f=b.formats[i];const auto& j=formats[i];f.struct_size=sizeof(f);f.sample_rate=j.at("rate").asUnsigned();f.channels=j.at("channels").asUnsigned(16);f.container_bits=j.at("containerBits").asUnsigned(64);f.valid_bits=j.at("validBits").asUnsigned(64);f.encoding=j.at("encoding").asUnsigned(2);if(!f.sample_rate||!f.channels||!f.valid_bits||f.valid_bits>f.container_bits||!f.encoding)throw std::invalid_argument("Invalid recorded format");}present[slot]=true;
 }
 const auto& domains=value.at("configuration").asArray();validateDomains(domains);for(unsigned slot=0;slot<domains.size();++slot)if(domains[slot].at("enabled").asBool()&&(!present[slot]||domains[slot].at("approvalRevision").asU64()!=candidate[slot].approval_revision))throw std::invalid_argument("Profile DAC approval mismatch");
 bindings_=candidate;present_=present;config_=domains; // Offline/changed bindings remain unresolved and DISARMED.
}
}
