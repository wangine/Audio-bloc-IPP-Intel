#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
class Capture final : public juce::AudioIODeviceCallback
{
public:
    explicit Capture(std::size_t capacityFrames)
        : samples(capacityFrames * 2, 0.0f), capacity(capacityFrames) {}

    void audioDeviceIOCallbackWithContext(
        const float* const* inputs, int inputCount,
        float* const*, int, int frames,
        const juce::AudioIODeviceCallbackContext&) override
    {
        const auto first = written.fetch_add(static_cast<std::size_t>(frames));
        const auto copyFrames = static_cast<int>(std::min<std::size_t>(
            static_cast<std::size_t>(frames), first < capacity ? capacity - first : 0));
        for (int frame = 0; frame < copyFrames; ++frame)
        {
            for (int channel = 0; channel < 2; ++channel)
                samples[(first + static_cast<std::size_t>(frame)) * 2 + channel] =
                    channel < inputCount && inputs[channel] != nullptr ? inputs[channel][frame] : 0.0f;
        }
        ++callbacks;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    std::vector<float> samples;
    std::atomic<std::size_t> written { 0 };
    std::atomic<std::uint64_t> callbacks { 0 };
    std::size_t capacity;
};

void analyse(const Capture& capture)
{
    const auto frames = std::min(capture.written.load(), capture.capacity);
    std::uint64_t normal = 0;
    std::uint64_t swapped = 0;
    std::uint64_t nonZero = 0;
    float peak = 0.0f;
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const float l = capture.samples[frame * 2];
        const float r = capture.samples[frame * 2 + 1];
        peak = std::max({peak, std::abs(l), std::abs(r)});
        nonZero += l != 0.0f || r != 0.0f;
        const auto a = static_cast<std::uint16_t>(
            static_cast<std::int16_t>(std::lrint(l * 32768.0f)));
        const auto b = static_cast<std::uint16_t>(
            static_cast<std::int16_t>(std::lrint(r * 32768.0f)));
        normal += a == 0xF872 && b == 0x4E1F;
        swapped += a == 0x72F8 && b == 0x1F4E;
    }
    std::cout << "Callbacks=" << capture.callbacks.load() << " frames=" << frames
              << " nonzero_frames=" << nonZero << " peak=" << peak << "\n"
              << "IEC61937 Pa/Pb normal=" << normal << " swapped=" << swapped << "\n";
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const int seconds = argc >= 2 ? std::max(1, std::atoi(argv[1])) : 10;
    juce::OwnedArray<juce::AudioIODeviceType> types;
    juce::AudioDeviceManager manager;
    manager.createAudioDeviceTypes(types);
    std::unique_ptr<juce::AudioIODevice> device;
    for (auto* type : types)
    {
        type->scanForDevices();
        const auto names = type->getDeviceNames(true);
        for (const auto& name : names)
        {
            std::cout << "[" << type->getTypeName() << "] " << name << "\n";
            if (name.containsIgnoreCase("UR23") || name.containsIgnoreCase("USB SPDIF Rx"))
            {
                device.reset(type->createDevice({}, name));
                break;
            }
        }
        if (device != nullptr)
            break;
    }
    if (device == nullptr)
    {
        std::cerr << "UR23 not found by JUCE\n";
        return 2;
    }
    juce::BigInteger inputs;
    inputs.setBit(0); inputs.setBit(1);
    juce::BigInteger outputs;
    const auto error = device->open(inputs, outputs, 48000.0, 512);
    std::cout << "Selected=" << device->getName() << " type=" << device->getTypeName()
              << " open_error='" << error << "' rate=" << device->getCurrentSampleRate()
              << " buffer=" << device->getCurrentBufferSizeSamples() << "\n";
    if (error.isNotEmpty())
        return 3;
    Capture capture(static_cast<std::size_t>(48000 * seconds + 8192));
    device->start(&capture);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    device->stop();
    analyse(capture);
    device->close();
    return 0;
}
