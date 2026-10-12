#include "PcmDecode.h"
#include <algorithm>
#include <cstring>
namespace entropy::music {
namespace {
constexpr uint8_t pcmGuid[] = {1, 0, 0, 0, 0, 0, 16, 0, 128, 0, 0, 170, 0, 56, 155, 113};
bool zero(const void *p, size_t n) {
    const auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; i++)
        if (b[i])
            return false;
    return true;
}
} // namespace
bool validFormat(const EntropyIngressFormatV2 &f) noexcept {
    const uint32_t rates[] = {44100, 48000, 88200, 96000, 176400, 192000};
    if (f.structSize != sizeof(f) || f.channels != 2 || f.channelMask != 3 ||
        !zero(f.reserved, sizeof(f.reserved)) ||
        std::find(std::begin(rates), std::end(rates), f.sampleRate) == std::end(rates))
        return false;
    if (f.representation == ENTROPY_REP_PCM) {
        if (f.formatTag != 0xfffe && f.formatTag != 1)
            return false;
        if (std::memcmp(f.subFormat, pcmGuid, 16))
            return false;
        if (!((f.containerBits == 16 && f.validBits == 16) || (f.containerBits == 32 && f.validBits == 24)))
            return false;
    } else if (f.representation == ENTROPY_REP_F64_PLANAR) {
        if (f.containerBits != 64 || f.validBits != 64 || f.formatTag || !zero(f.subFormat, 16))
            return false;
    } else
        return false;
    return f.blockAlign == f.containerBits / 8 * 2 && f.averageBytesPerSecond == f.sampleRate * f.blockAlign;
}
DecodeResult decodeStereo(const IngressPacketView &view, double *left, double *right,
                          uint32_t capacity) noexcept {
    const auto &p = view.header;
    const auto &f = p.format;
    if (p.abiVersion != ENTROPY_PROVIDER_ABI_V2 || p.structSize != sizeof(p) || !validFormat(f) || !left ||
        !right || !p.frames || p.frames > ENTROPY_MAX_PACKET_FRAMES || p.frames > capacity || !p.generation ||
        !p.qpcFrequency || !zero(p.reserved, sizeof(p.reserved)) || (p.flags & ~15u) ||
        p.mode > ENTROPY_MODE_EXCLUSIVE || p.payloadBytes != uint64_t(p.frames) * f.blockAlign ||
        view.payload.size() != p.payloadBytes)
        return {DecodeStatus::Invalid, 0};
    if (f.representation == ENTROPY_REP_F64_PLANAR) {
        if ((!p.originalContainerBits != !p.originalValidBits) ||
            p.originalValidBits > p.originalContainerBits || p.originalContainerBits > 64)
            return {DecodeStatus::Invalid, 0};
        if ((p.flags & ENTROPY_PACKET_RAW_IDENTITY) || !(p.flags & ENTROPY_PACKET_METADATA_PARTIAL))
            return {DecodeStatus::Invalid, 0};
        std::memcpy(left, view.payload.data(), p.frames * sizeof(double));
        std::memcpy(right, view.payload.data() + p.frames * sizeof(double), p.frames * sizeof(double));
    } else {
        if ((p.originalContainerBits || p.originalValidBits) &&
            (p.originalContainerBits != f.containerBits || p.originalValidBits != f.validBits))
            return {DecodeStatus::Invalid, 0};
        if ((p.flags & ENTROPY_PACKET_RAW_IDENTITY) &&
            (!p.packetSequence || (p.flags & ENTROPY_PACKET_METADATA_PARTIAL)))
            return {DecodeStatus::Invalid, 0};
        for (uint32_t i = 0; i < p.frames; i++)
            for (uint32_t c = 0; c < 2; c++) {
                double v;
                if (f.containerBits == 16) {
                    int16_t sample;
                    std::memcpy(&sample, view.payload.data() + (i * 2 + c) * 2, 2);
                    v = sample / 32768.0;
                } else {
                    int32_t sample;
                    std::memcpy(&sample, view.payload.data() + (i * 2 + c) * 4, 4);
                    v = (sample >> 8) / 8388608.0;
                }
                (c ? right : left)[i] = v;
            }
    }
    return {DecodeStatus::Ok, p.frames};
}
} // namespace entropy::music
