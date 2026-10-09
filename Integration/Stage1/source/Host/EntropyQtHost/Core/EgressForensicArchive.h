#pragma once
#include <entropy/egress_clock_diagnostics_abi.h>
#include <entropy/egress_forensic_abi.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <string>

namespace entropy::host {
// B2-FF-008 TEMP_FORENSIC. This object is owned EXCLUSIVELY by a non-RT collector.
// Never instantiate/use it in push/render. Retention is timestamped, bounded and
// explicitly reports queue loss; it is not a claim of a complete system timeline.
class EgressForensicArchive final {
public:
 static constexpr std::uint64_t eventHorizonNs=10000000000ull;
 static constexpr std::uint64_t trendHorizonNs=60000000000ull;
 void ingest(const EntropyForensicEvent& e) {
  if(e.struct_size!=sizeof(e) || (e.kind!=1 && e.kind!=2) || !e.enter_ns || !e.exit_ns || e.enter_ns>e.exit_ns){++invalidEvents_;return;}
  if(captured_){
   if(follow_.size()<128)follow_.push_back(e);else ++followDropped_;
   return;
  }
  auto& queue=events_[e.kind-1];queue.push_back(e);
  newestNs_=(std::max)(newestNs_,e.exit_ns);
  if(e.kind==ENTROPY_FORENSIC_RENDER && (!lastTrendNs_ || e.exit_ns-lastTrendNs_>=100000000ull)){
   trend_.push_back(e);lastTrendNs_=e.exit_ns;
  }
  for(auto& q:events_){
   while(!q.empty() && newestNs_-q.front().exit_ns>eventHorizonNs)q.pop_front();
   while(q.size()>8192){q.pop_front();++historyDropped_;}
  }
  while(!trend_.empty() && newestNs_-trend_.front().exit_ns>trendHorizonNs)trend_.pop_front();
  while(trend_.size()>1024){trend_.pop_front();++historyDropped_;}
 }
 bool capture(const EntropyEgressDiagnosticSnapshot& s,const EntropyForensicStatus& t) noexcept {
  if(captured_ || s.struct_size!=sizeof(s) || s.extension_version!=ENTROPY_EGRESS_DIAGNOSTICS_EXTENSION_VERSION ||
     s.publication_state!=ENTROPY_FF_READY || s.first_fault.struct_size!=sizeof(s.first_fault))return false;
  snapshot_=s;status_=t;captured_=true;return true;
 }
 bool captured()const noexcept{return captured_;}
 const EntropyEgressFirstFaultRecord& primary()const noexcept{return snapshot_.first_fault;}
 std::size_t eventCount()const noexcept{return events_[0].size()+events_[1].size();}
 std::size_t trendCount()const noexcept{return trend_.size();}
 std::size_t followOnCount()const noexcept{return follow_.size();}
 // Transactional directory commit: final directory never exposes partial files.
 // A failed export does not mutate/delete the in-memory record. Destination must
 // be unique; existing final or staging directory is an error, not an overwrite.
 bool exportIncident(const std::filesystem::path& destination,
                     const std::map<std::string,std::string>& metadata,std::string& error)const noexcept {
  std::filesystem::path staging;bool created=false;
  try {
   if(!captured_)throw std::runtime_error("no immutable first fault");
   if(std::filesystem::exists(destination))throw std::runtime_error("incident destination already exists");
   if(!destination.parent_path().empty())std::filesystem::create_directories(destination.parent_path());
   staging=destination;staging+=".partial";
   if(!std::filesystem::create_directory(staging))throw std::runtime_error("staging directory already exists");
   created=true;
   std::ofstream json(staging/"incident.json",std::ios::binary);
   json.exceptions(std::ios::badbit|std::ios::failbit);json.imbue(std::locale::classic());json<<std::setprecision(17);
   writeJson(json,metadata);json.flush();json.close();
   std::ofstream text(staging/"incident.txt",std::ios::binary);
   text.exceptions(std::ios::badbit|std::ios::failbit);text.imbue(std::locale::classic());
   const auto& r=primary();
   text<<"ENTROPY FORENSIC INCIDENT v1\nFIRST DETECTED FAULT, NOT PROVEN ROOT CAUSE\n"
       <<"namespace="<<r.fault_namespace<<" code="<<r.fault_code<<" origin="<<r.origin<<" site="<<r.site<<"\n"
       <<"monotonic_ns="<<r.monotonic_ns<<" instance="<<r.instance_id<<" lifecycle="<<r.lifecycle_id<<"\n"
       <<"write_used="<<r.write_frame_used<<" read_used="<<r.published_read_frame_used
       <<" retained="<<r.retained_used<<" incoming="<<r.incoming_frames<<" maximum_produced="<<r.maximum_produced_used
       <<" guard="<<r.guard_frames<<" capacity="<<r.capacity_frames<<"\n"
       <<"events="<<eventCount()<<" trend="<<trendCount()<<" follow_on="<<followOnCount()<<"\n"
       <<"push_dropped="<<status_.push_dropped<<" render_dropped="<<status_.render_dropped<<"\n"
       <<"See incident.json for exact event/control fields and validity.\n";
   for(const auto& [k,v]:metadata)text<<k<<"="<<v<<"\n";
   text.flush();text.close();
   std::filesystem::rename(staging,destination);error.clear();return true;
  }catch(const std::exception& e){
   error=e.what();
   if(created){std::error_code ec;std::filesystem::remove_all(staging,ec);}
   return false;
  }catch(...){error="unknown export failure";return false;}
 }
private:
 static void quoted(std::ostream& out,const std::string& s){
  static constexpr char hex[]="0123456789abcdef";out<<'"';
  for(unsigned char c:s){if(c=='"'||c=='\\'){out<<'\\'<<static_cast<char>(c);}else if(c<32){out<<"\\u00"<<hex[c>>4]<<hex[c&15];}else out<<static_cast<char>(c);}
  out<<'"';
 }
 static void number(std::ostream& out,double v){if(std::isfinite(v))out<<v;else out<<"null";}
 static void writeEvent(std::ostream& o,const EntropyForensicEvent& e){
  o<<"{\"kind\":"<<e.kind<<",\"sequence\":"<<e.sequence<<",\"valid_fields\":"<<e.valid_fields
   <<",\"enter_ns\":"<<e.enter_ns<<",\"exit_ns\":"<<e.exit_ns<<",\"flags\":"<<e.flags
   <<",\"sink_frame\":"<<e.sink_frame<<",\"device_host100ns\":"<<e.device_host100ns
   <<",\"nominal_count_observed\":"<<e.nominal_count_observed<<",\"output_count_observed\":"<<e.output_count_observed
   <<",\"requested_frames\":"<<e.requested_frames<<",\"consumed_frames\":"<<e.consumed_frames
   <<",\"produced_frames\":"<<e.produced_frames<<",\"state\":"<<e.state<<",\"api_result\":"<<e.api_result
   <<",\"clock_fault_flags\":"<<e.clock_fault_flags<<",\"last_fill_observed\":";number(o,e.last_fill_observed);
  if(!(e.valid_fields&ENTROPY_FORENSIC_CONTROL_CALLED)){o<<",\"control\":null}";return;}
  const auto& c=e.control;
  o<<",\"control\":{\"observe_calls\":"<<c.observe_calls<<",\"accepted_observations\":"<<c.accepted_observations
   <<",\"consecutive_rejections\":"<<c.consecutive_rejections<<",\"last_accepted_host100ns\":"<<c.last_accepted_host100ns
   <<",\"last_valid_host100ns\":"<<c.last_valid_host100ns<<",\"reset_count\":"<<c.reset_count
   <<",\"controller_calls\":"<<c.controller_calls<<",\"estimate_reason\":"<<c.estimate_reason
   <<",\"estimate_valid\":"<<c.estimate_valid<<",\"decision_updated\":"<<c.decision_updated<<",\"reset_site\":"<<c.reset_site;
#define B2_JSON_DOUBLE(name) o<<",\"" #name "\":";number(o,c.name)
  B2_JSON_DOUBLE(window_seconds);B2_JSON_DOUBLE(estimate_confidence);B2_JSON_DOUBLE(estimated_sink_hz);
  B2_JSON_DOUBLE(estimated_nominal_hz);B2_JSON_DOUBLE(base_ppm);B2_JSON_DOUBLE(backlog_ppm);
  B2_JSON_DOUBLE(requested_ppm);B2_JSON_DOUBLE(applied_ppm);B2_JSON_DOUBLE(fill_before);
  B2_JSON_DOUBLE(step_begin);B2_JSON_DOUBLE(step_end);
#undef B2_JSON_DOUBLE
  o<<"}}";
 }
 template<class Range>static void writeRange(std::ostream& o,const Range& range){bool first=true;o<<'[';for(const auto&e:range){if(!first)o<<',';first=false;writeEvent(o,e);}o<<']';}
 void writeJson(std::ostream& o,const std::map<std::string,std::string>& metadata)const {
  const auto&r=primary();o<<"{\"schema\":\"entropy.egress.forensic.incident.v1\",\"timebase\":\"module steady_clock nanoseconds; device_host100ns is separate\",\"primary_fault\":{";
#define B2_JSON_INT(name) o<<"\"" #name "\":"<<r.name<<','
  B2_JSON_INT(struct_size);B2_JSON_INT(record_version);B2_JSON_INT(fault_namespace);B2_JSON_INT(fault_code);
  B2_JSON_INT(origin);B2_JSON_INT(site);B2_JSON_INT(api_result);B2_JSON_INT(valid_fields);B2_JSON_INT(instance_id);
  B2_JSON_INT(lifecycle_id);B2_JSON_INT(first_event_id);B2_JSON_INT(monotonic_ns);B2_JSON_INT(config_revision);
  B2_JSON_INT(engine_rate);B2_JSON_INT(sink_rate);B2_JSON_INT(channels);B2_JSON_INT(write_frame_used);
  B2_JSON_INT(published_read_frame_used);B2_JSON_INT(retained_used);B2_JSON_INT(maximum_produced_used);
  B2_JSON_INT(producer_epoch);B2_JSON_INT(incoming_frames);B2_JSON_INT(block_flags);B2_JSON_INT(guard_frames);
  B2_JSON_INT(capacity_frames);B2_JSON_INT(target_frames);
#undef B2_JSON_INT
  o<<"\"last_render_snapshot_valid\":"<<r.last_render_snapshot_valid<<"},\"retention\":{\"event_horizon_ns\":"<<eventHorizonNs
   <<",\"trend_horizon_ns\":"<<trendHorizonNs<<",\"newest_event_ns\":"<<newestNs_
   <<",\"oldest_push_ns\":"<<(events_[0].empty()?0:events_[0].front().exit_ns)
   <<",\"oldest_render_ns\":"<<(events_[1].empty()?0:events_[1].front().exit_ns)
   <<",\"oldest_trend_ns\":"<<(trend_.empty()?0:trend_.front().exit_ns)
   <<",\"push_dropped\":"<<status_.push_dropped<<",\"render_dropped\":"<<status_.render_dropped
   <<",\"history_cap_dropped\":"<<historyDropped_<<",\"follow_on_dropped\":"<<followDropped_<<",\"invalid_events\":"<<invalidEvents_<<"},\"metadata\":{";
  bool first=true;for(const auto&[k,v]:metadata){if(!first)o<<',';first=false;quoted(o,k);o<<':';quoted(o,v);}o<<"},\"push_events\":";
  writeRange(o,events_[0]);o<<",\"render_events\":";writeRange(o,events_[1]);o<<",\"control_trend\":";writeRange(o,trend_);
  o<<",\"follow_on_events\":";writeRange(o,follow_);o<<"}\n";
 }
 std::array<std::deque<EntropyForensicEvent>,2> events_;
 std::deque<EntropyForensicEvent> trend_,follow_;
 EntropyEgressDiagnosticSnapshot snapshot_{};EntropyForensicStatus status_{};
 std::uint64_t newestNs_=0,lastTrendNs_=0,historyDropped_=0,followDropped_=0,invalidEvents_=0;
 bool captured_=false;
};
}
