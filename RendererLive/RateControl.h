#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace entropy::live {
// Sink owner only. Time comes from committed output frames, NOT callback QPC.
// Input/output ratio >1 consumes input faster. Positive backlog -> positive ppm.
class RateControl final {
public:
 void prepare(double target) noexcept {target_=target;integral_=filtered_=desired_=ppm_=weighted_=0;windowFrames_=updates_=0;}
 void observe(double totalUnsubmitted,unsigned outputFrames) noexcept {
  if(!outputFrames||!std::isfinite(totalUnsubmitted))return;
  weighted_+=totalUnsubmitted*outputFrames;windowFrames_+=outputFrames;
  if(windowFrames_>=192000){
   const double seconds=double(windowFrames_)/192000.0;
   const double error=weighted_/double(windowFrames_)-target_;
   filtered_+=(1.0-std::exp(-seconds/4.0))*(error-filtered_);
   const double candidate=std::clamp(integral_+0.003*filtered_*seconds,-800.0,800.0);
   const double command=0.2*filtered_+candidate;
   // Do not integrate further outwards against an already saturated command.
   if(std::abs(command)<=1000.0||command*filtered_<0.0)integral_=candidate;
   desired_=std::clamp(0.2*filtered_+integral_,-1000.0,1000.0);
   weighted_=0;windowFrames_=0;++updates_;
  }
  const double limit=20.0*double(outputFrames)/192000.0;
  ppm_+=std::clamp(desired_-ppm_,-limit,limit);
 }
 double ratio() const noexcept {return 1.0+ppm_*1e-6;}
 double ppm() const noexcept {return ppm_;}
 double requestedPpm() const noexcept {return desired_;}
 double filteredError() const noexcept {return filtered_;}
 uint64_t updates() const noexcept {return updates_;}
private:
 double target_=8192,integral_=0,filtered_=0,desired_=0,ppm_=0,weighted_=0;
 uint64_t windowFrames_=0,updates_=0;
};
}
