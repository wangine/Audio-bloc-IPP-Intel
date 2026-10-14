#include "Engine/Control/DeviceService.h"
#include <stdexcept>
namespace entropy::control {
ipc::Value DeviceService::independentPlaybackConfiguration() {
 using J=ipc::Value;
 changes_.ensure();
 const auto before=changes_.generation();
 refresh(); // inventory only; does not open a rendering stream
 J::Array result=config_.asArray();
 if(result.size()>2)throw std::invalid_argument("At most two DAC domains");
 unsigned valid=0;
 for(unsigned slot=0;slot<result.size();++slot){
  auto& domain=result[slot];
  domain["liveAllowed"]=false;domain["liveError"]="";
  if(!domain.at("enabled").asBool())continue;
  try {
   if(!present_[slot]||domain.at("logicalId").asString()!=(slot?"DAC_B":"DAC_A")||
      domain.at("approvalRevision").asU64()!=bindings_[slot].approval_revision)
    throw std::invalid_argument("Current explicit endpoint approval is required");
   const auto resolution=device::resolveBinding(bindings_[slot],inventory_.devices());
   const auto endpoint=domain.at("endpoint").asString();
   if(resolution.state!=ENTROPY_DEVICE_VERIFIED||endpoint!=resolution.resolved_endpoint_id_utf8)
    throw std::invalid_argument("Saved endpoint unavailable or identity unverified; no substitute selected");
   const auto channels=domain.at("channels").asUnsigned(16);
   const auto bits=domain.at("format").asUnsigned(1)?32u:24u;
   if(domain.at("rate").asUnsigned()!=192000||!channels||domain.at("lanes").asArray().size()!=channels)
    throw std::invalid_argument("Saved endpoint format/lane contract invalid");
   bool exact=false;
   for(const auto& found:inventory_.devices())if(endpoint==found.endpoint_id_utf8&&found.active)
    for(unsigned n=0;n<found.format_count;++n){const auto& f=found.formats[n];
     if(f.sample_rate==192000&&f.channels==channels&&f.valid_bits==24&&f.container_bits==bits&&f.encoding==ENTROPY_DEVICE_PCM_INTEGER)exact=true;
    }
   if(!exact)throw std::invalid_argument("Exact saved PCM24 format unsupported");
   if(!changes_.selectedActive(std::vector<std::string>{endpoint}))throw std::invalid_argument("Saved endpoint went offline");
   domain["liveAllowed"]=true;++valid;
  }catch(const std::exception& e){domain["liveError"]=e.what();}
 }
 if(before!=changes_.generation())throw std::runtime_error("Device topology changed during preflight; repeat explicit ARM");
 if(!valid)throw std::runtime_error("No approved enabled endpoint is currently available");
 // No configure(), binding update, profile rewrite or reassignment here.
 return J(std::move(result));
}
}
