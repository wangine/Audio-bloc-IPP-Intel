#pragma once
#include "../provider/BridgeClient.h"
#include <optional>
namespace entropy::music::audit {
struct StereoResult {uint64_t frames=0;double leftPeak=0,rightPeak=0,leftRms=0,rightRms=0,midRms=0,sideRms=0;std::optional<double> pearson,uncentered;};
class StereoMetrics {
    uint64_t n_=0;
    double l2_=0,r2_=0,m2_=0,s2_=0,lr_=0,lp_=0,rp_=0,lMean_=0,rMean_=0,lVariance_=0,rVariance_=0,covariance_=0;
public:
    void add(double left,double right) noexcept;
    bool accumulate(const EntropyIngressPacketV2&,const uint8_t* bytes);
    StereoResult result() const;
};
}
