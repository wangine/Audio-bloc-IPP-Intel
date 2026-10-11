#include "SourceDomain.h"
#include "InputFifo.h"
#include "MultiChannelSrcEngine.h"
#include "SourcePeakMeter.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include <mutex>
namespace entropy::music {
struct SourceDomain::Impl {
    entropy::src::MultiChannelSrcEngine src;
    SourcePeakMeter peakMeter;
    EntropyIngressFormatV2 format{};
    uint64_t generation{}, tokenCounter{}, lastSequence{}, lastFirst{}, lastCount{};
    std::atomic<uint64_t> activeToken{0}, snapshot{0};
    InputFifo fifo;
    StereoBlock pending;
    uint32_t pendingFrames{}, maxFrames{};
    bool first = true;
    std::atomic<uint32_t> clockPrimeFrames{0};
    std::atomic<bool> ingressRunning{true};
    uint64_t clockToken{};
    bool clockPrimed=false;
    uint64_t clockPrimeStarted{};
    std::vector<double> inL, inR, outL, outR;
    std::atomic<uint64_t> packets{0}, framesIn{0}, framesOut{0}, lost{0}, overruns{0}, underruns{0},
        faults{0}, telemetryGeneration{0}, startupFramesMeasured{0}, packetSequence{0}, firstSourceFrame{0},
        qpcTimestamp{0}, qpcFrequency{0};
    std::atomic<uint32_t> rate{0}, mode{0}, containerBits{0}, validBits{0}, representation{0},
        metadataFlags{0}, truePeakReady{0}, startupComplete{0};
    std::atomic<double> truePeakL{0}, truePeakR{0}, startupPeakL{0}, startupPeakR{0};
    std::array<std::atomic<double>, 12> meters{};
    // Used only by control/diagnostic readers. Ingress and the audio scheduler
    // never acquire this mutex or access this cached snapshot.
    mutable std::mutex telemetryReaderMutex;
    mutable SourceTelemetry lastTelemetry{};
    void begin() { snapshot.fetch_add(1, std::memory_order_acq_rel); }
    void end() { snapshot.fetch_add(1, std::memory_order_release); }
    void measure(const double *l, const double *r, uint32_t n, size_t offset) noexcept {
        double peakL = 0, peakR = 0, sumL = 0, sumR = 0, mid = 0, side = 0;
        for (uint32_t i = 0; i < n; i++) {
            auto a = l[i], b = r[i];
            peakL = (std::max)(peakL, std::abs(a));
            peakR = (std::max)(peakR, std::abs(b));
            sumL += a * a;
            sumR += b * b;
            mid += ((a + b) * .5) * ((a + b) * .5);
            side += ((a - b) * .5) * ((a - b) * .5);
        }
        double values[] = {peakL,
                           peakR,
                           n ? std::sqrt(sumL / n) : 0,
                           n ? std::sqrt(sumR / n) : 0,
                           n ? std::sqrt(mid / n) : 0,
                           n ? std::sqrt(side / n) : 0};
        for (size_t i = 0; i < 6; i++)
            meters[offset + i].store(values[i], std::memory_order_relaxed);
    }
};
SourceDomain::SourceDomain() : impl_(std::make_unique<Impl>()) {}
SourceDomain::~SourceDomain() = default;
bool SourceDomain::prepare(const EntropyIngressFormatV2 &f, uint64_t generation, uint32_t maximum) {
    if (!validFormat(f) || !generation || !maximum || maximum > ENTROPY_MAX_PACKET_FRAMES)
        return false;
    auto &p = *impl_;
    p.activeToken.store(0, std::memory_order_release);
    EntropySrcConfig cfg{};
    cfg.struct_size = sizeof(cfg);
    cfg.engine = ENTROPY_SRC_R8BRAIN_REFERENCE;
    cfg.source_rate = f.sampleRate;
    cfg.destination_rate = 192000;
    cfg.channels = 2;
    cfg.maximum_input_frames = maximum;
    if (!p.src.configure(cfg))
        return false;
    p.peakMeter.prepare(f.sampleRate, maximum);
    p.inL.resize(maximum);
    p.inR.resize(maximum);
    p.outL.resize(p.src.config().required_output_capacity);
    p.outR.resize(p.outL.size());
    p.format = f;
    p.generation = generation;
    p.maxFrames = maximum;
    p.pendingFrames = 0;
    p.first = true;
    p.clockPrimeFrames=0;
    p.ingressRunning.store(true,std::memory_order_release);
    p.lastSequence = p.lastFirst = p.lastCount = 0;
    p.begin();
    p.telemetryGeneration = generation;
    p.rate = f.sampleRate;
    p.containerBits = 0;
    p.validBits = 0;
    p.representation = f.representation;
    p.packetSequence = 0;
    p.firstSourceFrame = 0;
    p.qpcTimestamp = 0;
    p.qpcFrequency = 0;
    p.startupFramesMeasured = 0;
    p.truePeakReady = p.startupComplete = 0;
    p.truePeakL = p.truePeakR = p.startupPeakL = p.startupPeakR = 0;
    p.end();
    p.activeToken.store(++p.tokenCounter, std::memory_order_release);
    return true;
}
bool SourceDomain::processPacket(const IngressPacketView &v, WaitForSpace wait,
                                 void *waitContext) noexcept {
    auto &p = *impl_;
    if (v.header.generation != p.generation || std::memcmp(&p.format, &v.header.format, sizeof(p.format)) ||
        v.header.frames > p.maxFrames) {
        p.faults++;
        return false;
    }
    auto decoded = decodeStereo(v, p.inL.data(), p.inR.data(), p.maxFrames);
    if (!decoded) {
        p.faults++;
        return false;
    }
    const bool known = (v.header.flags & ENTROPY_PACKET_RAW_IDENTITY) != 0;
    const auto packetOutputFrames=(uint64_t(v.header.frames)*192000+p.format.sampleRate-1)/p.format.sampleRate;
    const auto prime=static_cast<uint32_t>((std::min)(uint64_t(16384),((packetOutputFrames+1023)/1024+1)*1024));
    p.clockPrimeFrames.store((std::max)(p.clockPrimeFrames.load(std::memory_order_relaxed),prime),std::memory_order_release);
    if (known && p.lastSequence && v.header.packetSequence <= p.lastSequence) {
        p.faults++;
        return false;
    }
    bool discontinuity = (v.header.flags & ENTROPY_PACKET_DISCONTINUITY) != 0;
    if (known && p.lastSequence &&
        (v.header.packetSequence != p.lastSequence + 1 ||
         v.header.firstSourceFrame != p.lastFirst + p.lastCount)) {
        if (!discontinuity) {
            p.faults++;
            return false;
        }
    }
    if (discontinuity && p.lastCount) {
        p.src.reset();
        p.peakMeter.reset();
        p.pendingFrames = 0;
        p.activeToken.store(++p.tokenCounter, std::memory_order_release);
        p.first = true;
    }
    p.lastSequence = v.header.packetSequence;
    p.lastFirst = v.header.firstSourceFrame;
    p.lastCount = v.header.frames;
    const double *inputs[] = {p.inL.data(), p.inR.data()};
    double *outputs[] = {p.outL.data(), p.outR.data()};
    EntropySrcBlockF64 b{};
    b.struct_size = sizeof(b);
    b.channels = 2;
    b.input_frames = decoded.frames;
    b.output_capacity = static_cast<uint32_t>(p.outL.size());
    b.inputs = inputs;
    b.outputs = outputs;
    if (p.src.process(b) != ENTROPY_OK) {
        p.faults++;
        return false;
    }
    auto peaks = p.peakMeter.process(p.inL.data(), p.inR.data(), b.input_frames, p.outL.data(), p.outR.data(),
                                     b.produced_frames);
    p.begin();
    p.truePeakL = peaks.truePeakL;
    p.truePeakR = peaks.truePeakR;
    p.startupPeakL = peaks.startupPeakL;
    p.startupPeakR = peaks.startupPeakR;
    p.truePeakReady = peaks.truePeakReady;
    p.startupComplete = peaks.startupComplete;
    p.startupFramesMeasured = peaks.startupFramesMeasured;
    p.packets++;
    p.framesIn += b.input_frames;
    p.framesOut += b.produced_frames;
    p.lost = v.header.lostFrames;
    p.mode = v.header.mode;
    p.containerBits = v.header.originalContainerBits;
    p.validBits = v.header.originalValidBits;
    if (p.format.representation == ENTROPY_REP_PCM && !p.validBits) {
        p.containerBits = p.format.containerBits;
        p.validBits = p.format.validBits;
    }
    p.metadataFlags = v.header.flags;
    p.packetSequence = v.header.packetSequence;
    p.firstSourceFrame = v.header.firstSourceFrame;
    p.qpcTimestamp = v.header.qpcTimestamp;
    p.qpcFrequency = v.header.qpcFrequency;
    p.measure(p.inL.data(), p.inR.data(), b.input_frames, 0);
    p.measure(p.outL.data(), p.outR.data(), b.produced_frames, 6);
    p.end();
    for (uint32_t i = 0; i < b.produced_frames; i++) {
        if (!p.pendingFrames)
            p.pending.status = {
                p.generation,        v.header.packetSequence, v.header.firstSourceFrame,
                v.header.lostFrames, p.format.sampleRate,     p.first ? ENTROPY_PACKET_DISCONTINUITY : 0u,
                v.header.mode,       p.containerBits.load(),  p.validBits.load()};
        p.pending.left[p.pendingFrames] = p.outL[i];
        p.pending.right[p.pendingFrames] = p.outR[i];
        if (++p.pendingFrames == 1024) {
            p.pendingFrames = 0;
            while (!p.fifo.push(p.pending, p.activeToken.load(std::memory_order_relaxed))) {
                if (!wait) {
                    p.overruns++;
                    p.faults++;
                    return false;
                }
                // Only an ingress worker opts into backpressure. Keep every
                // produced sample until the scheduler drains space, or cancel.
                if (!wait(waitContext))
                    return false;
            }
            p.first = false;
        }
    }
    return true;
}
bool SourceDomain::pop1024(StereoBlock &b) noexcept {
    if (impl_->fifo.pop(b, impl_->activeToken))
        return true;
    impl_->underruns++;
    return false;
}
void SourceDomain::setIngressRunning(bool running) noexcept {
    impl_->ingressRunning.store(running,std::memory_order_release);
}
bool SourceDomain::popClocked1024(StereoBlock &b,uint64_t now,uint64_t frequency) noexcept {
    auto& p=*impl_;
    const auto token=p.activeToken.load(std::memory_order_acquire);
    if(!token) {p.clockPrimed=false;p.clockToken=0;return false;}
    if(token!=p.clockToken) {p.clockToken=token;p.clockPrimed=false;p.clockPrimeStarted=0;}
    if(!p.clockPrimed) {
        const auto required=p.clockPrimeFrames.load(std::memory_order_acquire);
        const auto available=p.fifo.currentFill(p.activeToken,token);
        if(!required)return false;
        if(!available && !p.ingressRunning.load(std::memory_order_acquire)) {
            p.clockPrimeStarted=0;return false;
        }
        if(!p.clockPrimeStarted)p.clockPrimeStarted=now;
        const auto window=static_cast<uint64_t>((static_cast<long double>(required)*frequency+191999)/192000);
        const bool expired=frequency && now>=p.clockPrimeStarted && now-p.clockPrimeStarted>=window;
        if(available<required && !expired)return false;
        p.clockPrimed=true;
    }
    if(p.fifo.pop(b,p.activeToken,token))return true;
    if(!p.ingressRunning.load(std::memory_order_acquire)) {
        // The completed FIFO was drained. An explicit stopped stream has no
        // expected PCM. Resume uses the same existing prime window, no gain,
        // sample edits, discard, counter reset or timeout-based concealment.
        p.clockPrimed=false;p.clockPrimeStarted=0;return false;
    }
    if(token==p.activeToken.load(std::memory_order_acquire))p.underruns++;
    return false;
}
SourceTelemetry SourceDomain::telemetry() const noexcept {
    const auto &p = *impl_;
    std::lock_guard<std::mutex> reader(p.telemetryReaderMutex);
    SourceTelemetry t;
    for (unsigned attempt = 0; attempt < 4; attempt++) {
        auto seq = p.snapshot.load(std::memory_order_acquire);
        if (seq & 1)
            continue;
        t.generation = p.telemetryGeneration;
        t.packets = p.packets;
        t.framesIn = p.framesIn;
        t.framesOut = p.framesOut;
        t.lostFrames = p.lost;
        t.overruns = p.overruns;
        t.underruns = p.underruns;
        t.faults = p.faults;
        t.rate = p.rate;
        t.mode = p.mode;
        t.containerBits = p.containerBits;
        t.validBits = p.validBits;
        t.fillFrames = p.fifo.fill();
        t.representation = p.representation;
        t.metadataFlags = p.metadataFlags;
        t.packetSequence = p.packetSequence;
        t.firstSourceFrame = p.firstSourceFrame;
        t.qpcTimestamp = p.qpcTimestamp;
        t.qpcFrequency = p.qpcFrequency;
        t.truePeakL = p.truePeakL;
        t.truePeakR = p.truePeakR;
        t.startupPeakL = p.startupPeakL;
        t.startupPeakR = p.startupPeakR;
        t.truePeakReady = p.truePeakReady;
        t.startupComplete = p.startupComplete;
        t.startupFramesMeasured = p.startupFramesMeasured;
        double *values[] = {&t.prePeakL,  &t.prePeakR,   &t.preRmsL,    &t.preRmsR,
                            &t.preMidRms, &t.preSideRms, &t.postPeakL,  &t.postPeakR,
                            &t.postRmsL,  &t.postRmsR,   &t.postMidRms, &t.postSideRms};
        for (size_t i = 0; i < 12; i++)
            *values[i] = p.meters[i].load(std::memory_order_relaxed);
        if (seq == p.snapshot.load(std::memory_order_acquire)) {
            p.lastTelemetry=t;
            return t;
        }
    }
    return p.lastTelemetry;
}
uint32_t SourceDomain::queuedFrames() const noexcept { return impl_->fifo.fill(); }
void SourceDomain::invalidate(bool clearTelemetry) noexcept {
    auto &p = *impl_;
    p.activeToken.store(0, std::memory_order_release);
    if(!clearTelemetry)return;
    p.begin();
    p.telemetryGeneration = 0;
    p.rate = p.mode = p.containerBits = p.validBits = p.representation = p.metadataFlags = 0;
    p.packetSequence = p.firstSourceFrame = p.qpcTimestamp = p.qpcFrequency = 0;
    p.truePeakReady = p.startupComplete = 0;
    p.startupFramesMeasured = 0;
    p.truePeakL = p.truePeakR = p.startupPeakL = p.startupPeakR = 0;
    for (auto &value : p.meters)
        value = 0;
    p.end();
}
} // namespace entropy::music
