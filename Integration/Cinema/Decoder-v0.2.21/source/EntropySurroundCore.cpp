#include "EntropySurroundCore.h"

#include <algorithm>
#include <cmath>

namespace entropy::surround
{
IecStatus inspectIec61937(const std::uint8_t* bytes, std::size_t size) noexcept
{
    IecStatus result;
    if (bytes == nullptr) return result;
    for (std::size_t i = 0; i + 7 < size; ++i)
    {
        if (bytes[i] != 0x72 || bytes[i + 1] != 0xF8 ||
            bytes[i + 2] != 0x1F || bytes[i + 3] != 0x4E)
            continue;
        result.found = true;
        result.preambleOffset = i;
        result.dataType = static_cast<std::uint16_t>(bytes[i + 4] | (bytes[i + 5] << 8));
        result.payloadBits = static_cast<std::uint16_t>(bytes[i + 6] | (bytes[i + 7] << 8));
        switch (result.dataType & 0x1f)
        {
            case 1:  result.codec = Codec::ac3; break;
            case 11: result.codec = Codec::dtsType1; break;
            case 12: result.codec = Codec::dtsType2; break;
            case 13: result.codec = Codec::dtsType3; break;
            default: result.codec = Codec::unknownBurst; break;
        }
        return result;
    }
    return result;
}

std::string_view codecName(Codec codec) noexcept
{
    switch (codec)
    {
        case Codec::noSignal: return "NO SIGNAL - MUTED";
        case Codec::pcmStereo: return "PCM Stereo";
        case Codec::ac3: return "Dolby Digital (AC-3)";
        case Codec::dtsType1: return "DTS Type I";
        case Codec::dtsType2: return "DTS Type II";
        case Codec::dtsType3: return "DTS Type III";
        default: return "IEC61937 (unknown)";
    }
}

void applyDecodedCodecCalibration(std::array<float, 8>& frame,
                                  Codec codec) noexcept
{
    constexpr float dtsLfePlaybackGain = 3.1622776601683795f; // +10.0 dB
    if (codec == Codec::dtsType1 || codec == Codec::dtsType2 ||
        codec == Codec::dtsType3)
        frame[3] *= dtsLfePlaybackGain;
}

PcmPacketQuality classifyPcm16Stereo(const std::int16_t* samples,
                                     std::size_t frames) noexcept
{
    if (samples == nullptr || frames == 0) return PcmPacketQuality::silence;

    std::size_t nonZeroSamples = 0;
    std::size_t repeatedFrames = 0;
    long double energy = 0.0;
    long double differenceEnergy = 0.0;
    constexpr long double scale = 1.0L / 32768.0L;

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const auto left = samples[frame * 2];
        const auto right = samples[frame * 2 + 1];
        nonZeroSamples += left != 0;
        nonZeroSamples += right != 0;
        const auto l = static_cast<long double>(left) * scale;
        const auto r = static_cast<long double>(right) * scale;
        energy += l * l + r * r;
        if (frame != 0)
        {
            repeatedFrames += left == samples[(frame - 1) * 2] &&
                              right == samples[(frame - 1) * 2 + 1];
            const auto previousRight =
                static_cast<long double>(samples[(frame - 1) * 2 + 1]) * scale;
            differenceEnergy += (l - previousRight) * (l - previousRight);
        }
        differenceEnergy += (r - l) * (r - l);
    }

    if (nonZeroSamples == 0) return PcmPacketQuality::silence;

    // A healthy PCM stream does not freeze at a non-zero stereo sample. This
    // is the exact optical-unlocked signature captured from UR23: one stale
    // frame repeated for tens of milliseconds.
    if (frames >= 32 && repeatedFrames * 100 >= (frames - 1) * 95)
        return PcmPacketQuality::invalid;

    // Reject full-scale, noise-like USB/SPDIF unlock data. Requiring both high
    // signal energy and high adjacent-sample energy avoids rejecting a loud
    // but coherent sine wave or ordinary mastered PCM.
    const auto sampleCount = static_cast<long double>(frames * 2);
    const auto meanEnergy = energy / sampleCount;
    const auto meanDifferenceEnergy = differenceEnergy / sampleCount;
    if (meanEnergy > 0.12L && meanDifferenceEnergy > 0.30L)
        return PcmPacketQuality::invalid;

    return PcmPacketQuality::plausible;
}

void PcmLockGate::reset() noexcept
{
    plausibleFrames_ = 0;
    locked_ = false;
}

bool PcmLockGate::observe(const std::int16_t* samples,
                          std::size_t frames) noexcept
{
    switch (classifyPcm16Stereo(samples, frames))
    {
        case PcmPacketQuality::invalid:
            reset();
            return false;
        case PcmPacketQuality::silence:
            return locked_;
        case PcmPacketQuality::plausible:
            break;
    }

    plausibleFrames_ = std::min(confirmationFrames,
                                plausibleFrames_ + frames);
    locked_ = plausibleFrames_ >= confirmationFrames;
    return locked_;
}

void MatrixSurround::reset() noexcept
{
    delayL_.fill(0.0f);
    delayR_.fill(0.0f);
    delayPosition_ = 0;
    lfeState_ = correlation_ = leftEnergy_ = rightEnergy_ = 0.0f;
}

std::array<float, 8> MatrixSurround::process(float left, float right,
                                              bool output71) noexcept
{
    // Slow energy/correlation steering keeps dialog stable while preserving
    // strongly panned material in the front pair.
    constexpr float meterAlpha = 0.0025f;
    leftEnergy_ += meterAlpha * (left * left - leftEnergy_);
    rightEnergy_ += meterAlpha * (right * right - rightEnergy_);
    correlation_ += meterAlpha * (left * right - correlation_);
    const float denominator = std::sqrt(std::max(1.0e-12f, leftEnergy_ * rightEnergy_));
    const float normalizedCorrelation = std::clamp(correlation_ / denominator, -1.0f, 1.0f);
    const float centerSteer = std::clamp((normalizedCorrelation + 0.1f) / 1.1f, 0.0f, 1.0f);

    const float mid = 0.5f * (left + right);
    const float side = 0.5f * (left - right);
    const float center = mid * (0.45f + 0.35f * centerSteer);
    const float frontResidual = 0.78f - 0.20f * centerSteer;

    // A delayed, polarity-opposed difference signal gives classic matrix
    // surround behavior without claiming Dolby trademark/certification.
    const float delayedL = delayL_[delayPosition_];
    const float delayedR = delayR_[delayPosition_];
    delayL_[delayPosition_] = side;
    delayR_[delayPosition_] = -side;
    delayPosition_ = (delayPosition_ + 1) % delaySamples;
    const float surroundGain = 0.70f + 0.20f * (1.0f - centerSteer);

    // One-pole 120 Hz low-pass at 48 kHz, intentionally conservative.
    constexpr float lfeAlpha = 0.015585f;
    lfeState_ += lfeAlpha * (mid - lfeState_);

    std::array<float, 8> out {
        left * frontResidual,
        right * frontResidual,
        center,
        lfeState_ * 0.40f,
        delayedL * surroundGain,
        delayedR * surroundGain,
        0.0f,
        0.0f
    };
    if (output71)
    {
        out[4] *= 0.70710678f;
        out[5] *= 0.70710678f;
        out[6] = delayedR * surroundGain * 0.70710678f;
        out[7] = delayedL * surroundGain * 0.70710678f;
    }
    for (auto& sample : out)
        sample = std::clamp(sample, -1.0f, 1.0f);
    return out;
}
}
