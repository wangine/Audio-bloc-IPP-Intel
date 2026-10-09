#include "FractionalDelay.h"
#include <cmath>

namespace entropy::standalone {
FractionalDelayDesign FractionalDelayDesign::compile(int mode, double samples) noexcept {
    FractionalDelayDesign d;
    d.mode=std::clamp(mode,0,3); d.delay=std::max(0.0,samples);
    const int k=int(std::floor(d.delay)); const double f=d.delay-k;
    d.integerTap=k;
    if (!d.mode || f==0.0) return d;
    int n=d.mode==3?5:d.mode==2?3:1;
    // Causality/stability: D>N-1, and the ring's integer portion cannot be
    // negative. Short delays use the highest eligible odd order, explicitly
    // reported to telemetry; no latency is invented to support higher orders.
    if(n==5 && k<4) n=3;
    if(n==3 && k<2) n=1;
    d.order=n; d.integerTap=k-(n-1);
    constexpr int binomial[3][6]={{1,1,0,0,0,0},{1,3,3,1,0,0},{1,5,10,10,5,1}};
    const int row=(n-1)/2; const double D=(n-1)+f;
    for(int j=1;j<=n;++j) {
        double a=(j&1)?-double(binomial[row][j]):double(binomial[row][j]);
        for(int i=0;i<=n;++i) a *= (D-n+i)/(D-n+j+i);
        d.a[j]=a;
    }
    return d;
}
}
