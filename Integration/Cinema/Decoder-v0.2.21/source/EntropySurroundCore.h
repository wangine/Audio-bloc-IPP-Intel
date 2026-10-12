#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace entropy::surround
{
enum class Codec { noSignal, pcmStereo, ac3, dtsType1, dtsType2, dtsType3, unknownBurst };

struct IecStatus
{
    Codec codec = Codec::pcmStereo;
    std::uint16_t dataType = 0;
    std::uint16_t payloadBits = 0;
    std::size_t preambleOffset = 0;
    bool found = false;
};

IecStatus inspectIec61937(const std::uint8_t* bytes, std::size_t size) noexcept;
std::string_view codecName(Codec codec) noexcept;

// DTS carries LFE with cinema-reference attenuation. FFmpeg's DCA decoder
// exposes that attenuated sample value, while AC-3 returns playback level.
// Restore the DTS LFE playback calibration without changing any other codec
// or channel.
void applyDecodedCodecCalibration(std::array<float, 8>& frame,
                                  Codec codec) noexcept;

enum class PcmPacketQuality { silence, plausible, invalid };

// Classifies raw 48 kHz / 16-bit stereo capture packets before FULL AUTO is
// allowed to treat them as audio. USB SPDIF receivers can repeat one stale,
// non-zero sample indefinitely while optical lock is absent; that condition
// must never be sent to speakers as PCM.
PcmPacketQuality classifyPcm16Stereo(const std::int16_t* samples,
                                     std::size_t frames) noexcept;

class PcmLockGate
{
public:
    static constexpr std::size_t confirmationFrames = 12000; // 250 ms at 48 kHz

    void reset() noexcept;
    bool observe(const std::int16_t* samples, std::size_t frames) noexcept;
    bool locked() const noexcept { return locked_; }

private:
    std::size_t plausibleFrames_ = 0;
    bool locked_ = false;
};

// Channel order: FL, FR, FC, LFE, SL, SR, BL, BR.
class MatrixSurround
{
public:
    void reset() noexcept;
    std::array<float, 8> process(float left, float right, bool output71) noexcept;

private:
    static constexpr std::size_t delaySamples = 720; // 15 ms at 48 kHz
    std::array<float, delaySamples> delayL_ {};
    std::array<float, delaySamples> delayR_ {};
    std::size_t delayPosition_ = 0;
    float lfeState_ = 0.0f;
    float correlation_ = 0.0f;
    float leftEnergy_ = 0.0f;
    float rightEnergy_ = 0.0f;
};
}
