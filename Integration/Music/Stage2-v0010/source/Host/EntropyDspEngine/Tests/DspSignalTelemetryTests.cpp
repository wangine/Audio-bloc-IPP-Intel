#include "DspSignalTelemetry.h"
#include "TestSupport.h"
#include <limits>
int main(){Planes<10> input;Planes<11> output;DspSignalSnapshot signal{};
 input.data[0][17]=0.7;input.data[1][90]=-0.3;input.data[2][0]=2;output.data[0][17]=0.7;output.data[1][90]=-0.3;output.data[3][0]=2;
 measureSignal(signal,input.in.data(),output.in.data(),2,qpc());CHECK(signal.inputMask==3 && signal.outputMask==3);CHECK(signal.inputPeaks[2]==2 && signal.outputPeaks[3]==2);CHECK(!(signal.outputMask&4));
 measureSignal(signal,input.in.data(),output.in.data(),4,qpc());CHECK(signal.inputMask==4 && signal.outputMask==8);
 input.data[0][17]=std::numeric_limits<double>::infinity();input.data[1][90]=std::numeric_limits<double>::quiet_NaN();output.data[0][17]=std::numeric_limits<double>::infinity();output.data[1][90]=std::numeric_limits<double>::quiet_NaN();measureSignal(signal,input.in.data(),output.in.data(),7,qpc());CHECK(signal.inputMask==4 && signal.outputMask==8);
 auto masks=liveSignalMasks(signal,signal.publishedQpc+frequency()/2,frequency(),true);CHECK(masks.first==4 && masks.second==8);CHECK(liveSignalMasks(signal,signal.publishedQpc+frequency()/2+1,frequency(),true).first==0);CHECK(liveSignalMasks(signal,qpc(),frequency(),false).second==0);CHECK(liveSignalMasks(signal,signal.publishedQpc-1,frequency(),true).first==0);
 input.data={};output.data={};measureSignal(signal,input.in.data(),output.in.data(),7,qpc());CHECK(!signal.inputMask && !signal.outputMask);std::cout<<"PASS channel peaks, AUX-only music validity, Cinema mapping, nonfinite/zero/stale/fault/detach and reserved CX\n";
}
