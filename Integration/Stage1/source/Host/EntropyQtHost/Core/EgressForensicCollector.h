#pragma once
#include "EgressForensicArchive.h"
#include <array>
#include <stdexcept>
namespace entropy::host {
// B2-FF-008 TEMP_FORENSIC: SINGLE non-realtime owner. Call poll from an Engine
// worker (or offline harness worker), never the DSP cycle or the Qt reader.
// A caller owns the module lifetime; stop/join the worker before reset/destroy.
class EgressForensicCollector final {
public:
 EgressForensicCollector(EntropyModuleHandle handle,EntropyEgressDiagnosticsApiV1 compact,
                        EntropyEgressForensicApiV1 trace)
     :handle_(handle),compact_(compact),trace_(trace){
  if(!handle_ || !compact_.get_snapshot || !trace_.drain || !trace_.get_status ||
     compact_.extension_version!=ENTROPY_EGRESS_DIAGNOSTICS_EXTENSION_VERSION ||
     trace_.extension_version!=ENTROPY_EGRESS_FORENSIC_VERSION)throw std::invalid_argument("forensic API unavailable/incompatible");
 }
 // A bounded drain pass; queue loss is visible in status. Export is separate so
 // disk failure/delay never occupies an audio owner and does not erase evidence.
 bool poll() {
  for(std::uint32_t kind=1;kind<=2;++kind){
   for(unsigned batch=0;batch<16;++batch){
    std::uint32_t count=0;
    if(trace_.drain(handle_,kind,events_.data(),static_cast<std::uint32_t>(events_.size()),&count)!=ENTROPY_OK || count>events_.size())throw std::runtime_error("forensic drain failed");
    for(std::uint32_t i=0;i<count;++i)archive_.ingest(events_[i]);
    if(count<events_.size())break;
   }
  }
  EntropyForensicStatus status{};status.struct_size=sizeof(status);
  if(trace_.get_status(handle_,&status)!=ENTROPY_OK || status.version!=ENTROPY_EGRESS_FORENSIC_VERSION || status.struct_size!=sizeof(status))throw std::runtime_error("forensic status failed");
  EntropyEgressDiagnosticSnapshot snapshot{};snapshot.struct_size=sizeof(snapshot);
  if(compact_.get_snapshot(handle_,&snapshot)!=ENTROPY_OK)throw std::runtime_error("first fault snapshot failed");
  archive_.capture(snapshot,status);return archive_.captured();
 }
 bool pollAndExport(const std::filesystem::path& destination,const std::map<std::string,std::string>& metadata,std::string& error){
  poll();if(exported_)return true;
  if(!archive_.captured()){error.clear();return false;}
  exported_=archive_.exportIncident(destination,metadata,error);return exported_;
 }
 bool exported()const noexcept{return exported_;}
 const EgressForensicArchive& archive()const noexcept{return archive_;}
private:
 EntropyModuleHandle handle_{};EntropyEgressDiagnosticsApiV1 compact_{};EntropyEgressForensicApiV1 trace_{};
 std::array<EntropyForensicEvent,256> events_{};EgressForensicArchive archive_;bool exported_=false;
};
}
