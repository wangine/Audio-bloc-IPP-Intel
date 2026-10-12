#pragma once
#include <algorithm>
#include <cmath>

// Measures the actual decoder -> SourceDomain -> unity matrix path, not a DSP mock.
inline void levelAcceptance() {
    constexpr double pi = 3.1415926535897932384626433832795;
    auto peak = [](const std::vector<double> &v) {
        double p = 0;
        for (auto x : v)
            p = std::max(p, std::abs(x));
        return p;
    };
    auto stablePeak = [](const std::vector<double> &v) {
        double p = 0;
        for (size_t i = 8192; i < v.size() - 8192; i++)
            p = std::max(p, std::abs(v[i]));
        return p;
    };
    auto peakIndex = [](const std::vector<double> &v) {
        size_t n = 0;
        for (size_t i = 1; i < v.size(); i++)
            if (std::abs(v[i]) > std::abs(v[n]))
                n = i;
        return n;
    };
    auto amplitude = [&](const std::vector<double> &v, double frequency, double rate, size_t skip) {
        double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0, sum = 0;
        size_t end = v.size() - skip;
        for (size_t i = skip; i < end; i++) {
            double angle = 2 * pi * frequency * i / rate, s = std::sin(angle), c = std::cos(angle);
            ss += s * s;
            cc += c * c;
            sc += s * c;
            ys += v[i] * s;
            yc += v[i] * c;
            sum += v[i];
        }
        if (!frequency)
            return std::abs(sum / (end - skip));
        double det = ss * cc - sc * sc, a = (ys * cc - yc * sc) / det, b = (yc * ss - ys * sc) / det;
        return std::hypot(a, b);
    };
    std::ofstream report(std::filesystem::path(TEST_EVIDENCE_DIR) / "level-results.csv");
    CHECK(report);
    report << std::setprecision(15)
           << "rate,bits,fixture,input_peak_L,output_peak_L,input_peak_R,output_peak_R,gain_dB_L,gain_dB_R,"
              "over_0dBFS,steady_peak_L,steady_peak_R,peak_output_index_L,true_peak_estimate_L,true_peak_"
              "estimate_R,src_startup_peak_L,src_startup_peak_R\n";
    for (unsigned rate : {44100u, 48000u, 88200u, 96000u, 176400u, 192000u})
        for (unsigned bits : {16u, 24u})
            for (unsigned fixture = 0; fixture < 4; fixture++) {
                const char *names[] = {"DC_-6dBFS", "1k_-1dBFS", "18k_-0.1dBFS", "intersample_Fs4"};
                double frequency = fixture == 0 ? 0 : fixture == 1 ? 1000 : fixture == 2 ? 18000 : rate / 4.0;
                double level = fixture == 0   ? .5
                               : fixture == 1 ? std::pow(10., -1. / 20)
                               : fixture == 2 ? std::pow(10., -.1 / 20)
                                              : 1.3;
                auto f = pcmFormat(rate, bits == 16 ? 16 : 32, bits);
                SourceDomain domain;
                CHECK(domain.prepare(f, 1, 1024));
                InputMatrix matrix;
                StereoBlock block, absent, out;
                std::vector<double> in[2], output[2];
                const double scale = bits == 16 ? 32768. : 8388608.;
                for (unsigned packet = 0; packet < 32; packet++) {
                    std::vector<std::byte> payload(1024 * f.blockAlign);
                    for (unsigned i = 0; i < 1024; i++)
                        for (unsigned c = 0; c < 2; c++) {
                            double angle =
                                2 * pi * frequency * (packet * 1024 + i) / rate + (fixture == 3 ? pi / 4 : 0);
                            double value =
                                fixture == 0 ? (c ? -.25 : .5) : level * (c ? -.5 : 1) * std::sin(angle);
                            int32_t sample = static_cast<int32_t>(std::llround(value * scale));
                            CHECK(sample < scale && sample >= -scale);
                            in[c].push_back(sample / scale);
                            if (bits == 16) {
                                auto small = static_cast<int16_t>(sample);
                                memcpy(payload.data() + (i * 2 + c) * 2, &small, 2);
                            } else {
                                int32_t packed = sample * 256;
                                memcpy(payload.data() + (i * 2 + c) * 4, &packed, 4);
                            }
                        }
                    EntropyIngressPacketV2 p{};
                    p.abiVersion = ENTROPY_PROVIDER_ABI_V2;
                    p.structSize = sizeof(p);
                    p.format = f;
                    p.generation = 1;
                    p.packetSequence = packet + 1;
                    p.firstSourceFrame = packet * 1024;
                    p.frames = 1024;
                    p.payloadBytes = static_cast<uint32_t>(payload.size());
                    p.qpcFrequency = 10000000;
                    p.qpcTimestamp = packet + 1;
                    p.flags = ENTROPY_PACKET_RAW_IDENTITY;
                    p.originalContainerBits = f.containerBits;
                    p.originalValidBits = bits;
                    CHECK(domain.processPacket({p, payload}));
                    while (domain.pop1024(block)) {
                        matrix.process(block, absent, out, 1);
                        output[0].insert(output[0].end(), out.left.begin(), out.left.end());
                        output[1].insert(output[1].end(), out.right.begin(), out.right.end());
                    }
                }
                double gains[2];
                for (unsigned c = 0; c < 2; c++) {
                    CHECK(output[c].size() > 16384);
                    gains[c] = 20 * std::log10(amplitude(output[c], frequency, 192000, 8192) /
                                               amplitude(in[c], frequency, rate, 4096));
                    CHECK(std::abs(gains[c]) < .002);
                    if (rate == 192000) {
                        CHECK(output[c].size() == in[c].size());
                        CHECK(!memcmp(output[c].data(), in[c].data(), in[c].size() * 8));
                    }
                }
                auto maximum = std::max(peak(output[0]), peak(output[1]));
                CHECK(peak(in[0]) < 1 && peak(in[1]) < 1);
                if (fixture == 3 && rate != 192000)
                    CHECK(maximum > 1.1); // Reconstruction overshoot is preserved, never clipped.
                if (fixture == 3 && rate != 192000)
                    CHECK(stablePeak(output[0]) <= 1.30005);
                const auto telemetry = domain.telemetry();
                CHECK(telemetry.truePeakReady && telemetry.startupComplete &&
                      telemetry.startupFramesMeasured == 8192);
                if (fixture == 3)
                    CHECK(std::abs(telemetry.truePeakL - 1.3) < 1e-4);
                report << rate << ',' << bits << ',' << names[fixture] << ',' << peak(in[0]) << ','
                       << peak(output[0]) << ',' << peak(in[1]) << ',' << peak(output[1]) << ',' << gains[0]
                       << ',' << gains[1] << ',' << (maximum > 1) << ',' << stablePeak(output[0]) << ','
                       << stablePeak(output[1]) << ',' << peakIndex(output[0]) << ',' << telemetry.truePeakL
                       << ',' << telemetry.truePeakR << ',' << telemetry.startupPeakL << ','
                       << telemetry.startupPeakR << '\n';
            }
    // Single-source unity must also preserve signed zero and finite overrange FP64.
    StereoBlock m, a, o;
    for (unsigned i = 0; i < 1024; i++) {
        m.left[i] = i % 2 ? 1.25 : -0.0;
        m.right[i] = i % 2 ? -1.75 : .5;
    }
    InputMatrix matrix;
    matrix.process(m, a, o, 1);
    CHECK(!memcmp(m.left.data(), o.left.data(), 8192) && !memcmp(m.right.data(), o.right.data(), 8192));
    matrix.process(a, m, o, 2);
    CHECK(!memcmp(m.left.data(), o.left.data(), 8192));
    std::cout << "PASS level: 48 raw PCM fixtures, six rates x two word lengths x four signals; unity gain "
                 "<0.002 dB; exact 192k samples; unclipped reconstructed peaks\n";
}
