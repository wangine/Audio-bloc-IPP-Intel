#pragma once
#include "SourcePeakMeter.h"
inline void peakMeterAcceptance() {
    SourcePeakMeter meter;
    meter.prepare(48000, 1024);
    std::array<double, 1024> l{}, r{}, outL{}, outR{};
    PeakReading reading;
    // Source Fs/4 sinusoid: samples 1.3/sqrt(2), true continuous amplitude 1.3.
    for (unsigned i = 0; i < 1024; i++) {
        l[i] = 1.3 * std::sin(3.14159265358979323846 * i / 2 + 3.14159265358979323846 / 4);
        r[i] = -.5 * l[i];
        outL[i] = .5;
        outR[i] = -.25;
    }
    outL[10] = 1.3134740784985;
    for (unsigned n = 0; n < 16; n++)
        reading = meter.process(l.data(), r.data(), 1024, outL.data(), outR.data(), 1024);
    CHECK(reading.truePeakReady && reading.startupComplete);
    CHECK(std::abs(reading.truePeakL - 1.3) < 1e-7 && std::abs(reading.truePeakR - .65) < 1e-7);
    CHECK(reading.startupFramesMeasured == 8192 && reading.startupPeakL == outL[10] &&
          reading.startupPeakR == .25);
    meter.reset();
    reading = meter.process(l.data(), r.data(), 1024, outL.data(), outR.data(), 1024);
    CHECK(!reading.truePeakReady && !reading.startupComplete && reading.startupFramesMeasured == 1024);
    std::cout << "PASS separate input sample peak / 4x estimated true peak / latched SRC startup peak; reset "
                 "and settling state\n";
}
