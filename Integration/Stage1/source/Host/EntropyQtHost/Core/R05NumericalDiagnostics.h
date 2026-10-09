#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
namespace entropy::r05 {
inline constexpr double anomalyFloor=1e-4,anomalyMultiplier=16.0;
inline constexpr unsigned maxChannels=16;
enum class Stage:unsigned {SourceMusic,SourceAux,SourceCinema,InputMatrixOut,ProgramMixerOut,LoudnessIn,LoudnessOut,EqIn,EqOut,WireIn,WireOut,CrossoverIn,CrossoverOut,OutputMatrixIn,OutputMatrixOut,HeadroomIn,HeadroomOut,ClockIn,ClockOut,Count};
inline constexpr unsigned stageCount=unsigned(Stage::Count);
inline constexpr const char* stageNames[]={"source_src_music_out","source_src_aux_out","source_src_cinema_out","input_matrix_out","program_mixer_out","loudness_in","loudness_out","parametric_eq_in","parametric_eq_out","expansion_wire_in","expansion_wire_out","crossover_in","crossover_out","output_matrix_in","output_matrix_out","headroom_in","headroom_out","clock_sync_A","clock_sync_B"};
struct Metric {
 double first=0,last=0,peak=0,peak_signed=0,boundary_delta=0,boundary_second=0;
 double anomaly_signed=0,anomaly_derivative=0,anomaly_second=0,derivative_context=0,second_context=0;
 std::int32_t peak_index=-1,first_overrange=-1,anomaly_index=-1;
 std::uint32_t anomaly_kind=0,nonfinite=0,history_valid=0,valid=0;
};
class Analyzer {
 struct History {std::array<double,10> samples{};unsigned count=0;double derivativeEma=0,secondEma=0;};
 std::array<History,maxChannels> history_{};
 static void updateEma(double& ema,double difference)noexcept {
  // Keep the /32 recurrence normal even with FTZ/DAZ disabled. This bound is
  // far below anomalyFloor/16: preserve useful history and threshold semantics,
  // including small signals returning after silence. Only EMA state/input is
  // bounded; samples, peaks, differences and anomaly context stay exact.
  constexpr double minimumEma=std::numeric_limits<double>::min()*64.0;
  const double magnitude=std::abs(difference);
  const double input=magnitude<minimumEma?0.0:magnitude;
  if(input==0.0&&ema==0.0)return;
  ema+=(input-ema)/32.0;
  if(ema<minimumEma)ema=0.0;
 }
public:
 void reset()noexcept {history_={};}
 Metric measure(unsigned channel,const double* p,unsigned frames)noexcept {
  Metric m{};if(channel>=maxChannels)return m;auto& h=history_[channel];
  if(!p||!frames){h={};return m;}m.valid=1;m.first=p[0];m.last=p[frames-1];
  const auto sample=[&](int i,double& v)noexcept{
   if(i>=0){if(unsigned(i)>=frames)return false;v=p[i];}
   else {if(unsigned(-i)>h.count)return false;v=h.samples[h.count+i];}
   return std::isfinite(v);
  };
  const auto difference=[&](int i,unsigned order,double& d)noexcept{
   double a,b,c;if(!sample(i,a)||!sample(i-1,b))return false;
   d=a-b;if(order==2){if(!sample(i-2,c))return false;d-=b-c;}return std::isfinite(d);
  };
  double bd=0,bs=0;m.history_valid=difference(0,1,bd)&&difference(0,2,bs);
  if(m.history_valid){m.boundary_delta=bd;m.boundary_second=bs;}
  for(unsigned i=0;i<frames;++i){
   if(!std::isfinite(p[i])){++m.nonfinite;continue;}
   const double av=std::abs(p[i]);if(m.peak_index<0||av>m.peak){m.peak=av;m.peak_signed=p[i];m.peak_index=int(i);}
   if(av>1.0&&m.first_overrange<0)m.first_overrange=int(i);
   double d=0,s=0;const bool dv=difference(int(i),1,d),sv=difference(int(i),2,s);
   if(m.anomaly_index<0&&((dv&&std::abs(d)>(std::max)(anomalyFloor,anomalyMultiplier*h.derivativeEma))||
      (sv&&std::abs(s)>(std::max)(anomalyFloor,anomalyMultiplier*h.secondEma)))){
    double dc=0,sc=0;unsigned dn=0,sn=0;
    // Symmetric surrounding difference context; exclude the candidate's +/-2-frame impulse footprint.
    for(int k=-8;k<=8;++k){if(k>-3&&k<3)continue;double v;
     if(difference(int(i)+k,1,v)){dc+=std::abs(v);++dn;}
     if(difference(int(i)+k,2,v)){sc+=std::abs(v);++sn;}}
    if(dn)dc/=dn;if(sn)sc/=sn;
    unsigned kind=0;if(dv&&dn>=6&&std::abs(d)>(std::max)(anomalyFloor,anomalyMultiplier*dc))kind|=1;
    if(sv&&sn>=6&&std::abs(s)>(std::max)(anomalyFloor,anomalyMultiplier*sc))kind|=2;
    if(kind){m.anomaly_index=int(i);m.anomaly_kind=kind;m.anomaly_signed=p[i];m.anomaly_derivative=d;m.anomaly_second=s;m.derivative_context=dc;m.second_context=sc;}
   }
   if(dv)updateEma(h.derivativeEma,d);
   if(sv)updateEma(h.secondEma,s);
  }
  if(frames>=10){std::copy_n(p+frames-10,10,h.samples.begin());h.count=10;}
  else {for(unsigned i=0;i<frames;++i){if(h.count==10){for(unsigned j=1;j<10;++j)h.samples[j-1]=h.samples[j];--h.count;}h.samples[h.count++]=p[i];}}
  return m;
 }
};
struct Block {std::uint64_t id=0,frame_offset=0;std::array<std::uint64_t,3> generation{};std::uint32_t frames=0;};
struct Event {Stage stage{};std::uint32_t channel=0,lane=0,flags=0;std::uint64_t qpc=0;Metric metric{};};
struct Packet {
 Block block{};std::uint64_t session=0,sequence=0,clock_input_offset=0;
 double read_begin=0,read_end=0,step_begin=0,step_end=0;
 std::uint32_t domain=0,frames=0,count=0,fault=0,phase_valid=0,sink_rate=0;
 std::int32_t result=0;
 std::array<Event,160> events{};
};
}
