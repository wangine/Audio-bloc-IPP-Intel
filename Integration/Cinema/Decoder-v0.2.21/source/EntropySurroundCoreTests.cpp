#include "EntropySurroundCore.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    using namespace entropy::surround;
    const std::uint8_t ac3[] {0, 0, 0x72, 0xF8, 0x1F, 0x4E, 1, 0, 0, 0x50};
    const auto a = inspectIec61937(ac3, sizeof(ac3));
    require(a.found && a.codec == Codec::ac3 && a.payloadBits == 20480,
            "AC-3 IEC61937 detection failed");
    const std::uint8_t dts[] {0x72, 0xF8, 0x1F, 0x4E, 11, 0, 0xE0, 0x3E};
    const auto d = inspectIec61937(dts, sizeof(dts));
    require(d.found && d.codec == Codec::dtsType1 && d.payloadBits == 16096,
            "DTS IEC61937 detection failed");

    std::array<float, 8> decodedFrame {0.1f, 0.2f, 0.3f, 0.1f,
                                       0.4f, 0.5f, 0.0f, 0.0f};
    const auto original = decodedFrame;
    applyDecodedCodecCalibration(decodedFrame, Codec::dtsType1);
    require(std::abs(decodedFrame[3] - 0.316227766f) < 1.0e-6f,
            "DTS LFE +10 dB calibration failed");
    require(decodedFrame[0] == original[0] && decodedFrame[1] == original[1] &&
            decodedFrame[2] == original[2] && decodedFrame[4] == original[4] &&
            decodedFrame[5] == original[5],
            "DTS calibration changed a non-LFE channel");
    decodedFrame = original;
    applyDecodedCodecCalibration(decodedFrame, Codec::ac3);
    require(decodedFrame == original, "AC-3 was changed by DTS calibration");

    std::array<std::int16_t, 960> silence{};
    require(classifyPcm16Stereo(silence.data(), 480) == PcmPacketQuality::silence,
            "Silence classification failed");

    std::array<std::int16_t, 960> frozen{};
    for (std::size_t i = 0; i < frozen.size(); i += 2)
    {
        frozen[i] = -8448;
        frozen[i + 1] = -16226;
    }
    require(classifyPcm16Stereo(frozen.data(), 480) == PcmPacketQuality::invalid,
            "UR23 frozen optical-loss frame was accepted as PCM");

    std::array<std::int16_t, 960> pcm{};
    for (std::size_t frame = 0; frame < 480; ++frame)
    {
        const auto sample = static_cast<std::int16_t>(
            std::sin(frame * 0.071) * 12000.0);
        pcm[frame * 2] = sample;
        pcm[frame * 2 + 1] = static_cast<std::int16_t>(sample / 2);
    }
    require(classifyPcm16Stereo(pcm.data(), 480) == PcmPacketQuality::plausible,
            "Coherent PCM was rejected");
    PcmLockGate pcmGate;
    for (std::size_t frames = 0; frames < PcmLockGate::confirmationFrames;
         frames += 480)
    {
        const bool expected = frames + 480 >= PcmLockGate::confirmationFrames;
        require(pcmGate.observe(pcm.data(), 480) == expected,
                "PCM confirmation timing failed");
    }
    require(pcmGate.locked(), "PCM gate did not lock");
    require(!pcmGate.observe(frozen.data(), 480),
            "Frozen frame did not mute a locked PCM stream");
    require(!pcmGate.locked(), "Frozen frame did not clear PCM lock");

    MatrixSurround upmixer;
    std::array<float, 8> last{};
    for (int i = 0; i < 2000; ++i) last = upmixer.process(0.5f, 0.5f, false);
    require(std::abs(last[2]) > std::abs(last[4]), "Center steering failed");
    require(last[6] == 0.0f && last[7] == 0.0f, "5.1 rear channels are not silent");
    upmixer.reset();
    for (int i = 0; i < 2000; ++i) last = upmixer.process(0.5f, -0.5f, true);
    require(std::abs(last[4]) > 0.1f && std::abs(last[6]) > 0.1f,
            "7.1 surround steering failed");
    std::cout << "Entropy Surround core tests: PASS\n";
}
