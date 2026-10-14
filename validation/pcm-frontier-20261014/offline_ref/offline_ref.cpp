#include "MultiChannelSrcEngine.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t le16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
int32_t pcm24(const uint8_t* p) {
    const auto value = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
    return static_cast<int32_t>((value & 0x800000u) ? (value | 0xff000000u) : value);
}
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "usage: offline_ref reference-24-96.wav offline-r8brain-fp64.bin");
        std::ifstream in(argv[1], std::ios::binary);
        require(bool(in), "cannot open WAV");
        std::array<uint8_t, 44> wav{};
        in.read(reinterpret_cast<char*>(wav.data()), wav.size());
        require(in.gcount() == static_cast<std::streamsize>(wav.size()), "short WAV header");
        require(!std::memcmp(wav.data(), "RIFF", 4) && !std::memcmp(wav.data()+8, "WAVEfmt ", 8) &&
                    !std::memcmp(wav.data()+36, "data", 4), "unexpected WAV header");
        require(le32(wav.data()+16) == 16 && le16(wav.data()+20) == 1 &&
                    le16(wav.data()+22) == 2 && le32(wav.data()+24) == 96000 &&
                    le16(wav.data()+32) == 6 && le16(wav.data()+34) == 24,
                "expected PCM24 packed stereo 96 kHz");
        const uint32_t bytes = le32(wav.data()+40);
        require(bytes == 5184000u * 6u, "reference frame count differs");

        entropy::src::MultiChannelSrcEngine src;
        EntropySrcConfig config{};
        config.struct_size = sizeof(config);
        config.engine = ENTROPY_SRC_R8BRAIN_REFERENCE;
        config.source_rate = 96000;
        config.destination_rate = 192000;
        config.channels = 2;
        config.maximum_input_frames = 1024;
        require(src.configure(config), "r8brain configure failed");
        std::array<double, 1024> inL{}, inR{};
        std::vector<double> outL(src.config().required_output_capacity);
        std::vector<double> outR(src.config().required_output_capacity);
        const double* inputs[]{inL.data(), inR.data()};
        double* outputs[]{outL.data(), outR.data()};
        std::vector<double> allL, allR;
        allL.reserve(10368000); allR.reserve(10368000);
        std::vector<uint8_t> packet(1024 * 6);
        uint32_t consumed = 0;
        while (consumed < 5184000u) {
            const uint32_t frames = std::min(1024u, 5184000u - consumed);
            in.read(reinterpret_cast<char*>(packet.data()), frames * 6);
            require(in.gcount() == static_cast<std::streamsize>(frames * 6), "short WAV data");
            for (uint32_t i = 0; i < frames; ++i) {
                inL[i] = double(pcm24(packet.data() + i * 6)) / 8388608.0;
                inR[i] = double(pcm24(packet.data() + i * 6 + 3)) / 8388608.0;
            }
            EntropySrcBlockF64 block{};
            block.struct_size = sizeof(block);
            block.channels = 2;
            block.input_frames = frames;
            block.output_capacity = static_cast<uint32_t>(outL.size());
            block.inputs = inputs;
            block.outputs = outputs;
            require(src.process(block) == ENTROPY_OK && block.consumed_frames == frames,
                    "r8brain processing failed");
            allL.insert(allL.end(), outL.begin(), outL.begin() + block.produced_frames);
            allR.insert(allR.end(), outR.begin(), outR.begin() + block.produced_frames);
            consumed += frames;
        }
        // r8brain holds an output tail. Feed declared zero padding after the
        // 54-second master, then keep exactly 54 seconds at 192 kHz. The WAV
        // itself ends with two seconds of digital silence; this padding is
        // recorded rather than hidden by a per-block realignment.
        uint32_t paddingInputFrames = 0;
        while (allL.size() < 10368000u) {
            inL.fill(0.0); inR.fill(0.0);
            EntropySrcBlockF64 block{};
            block.struct_size = sizeof(block);
            block.channels = 2;
            block.input_frames = 1024;
            block.output_capacity = static_cast<uint32_t>(outL.size());
            block.inputs = inputs;
            block.outputs = outputs;
            require(src.process(block) == ENTROPY_OK && block.consumed_frames == 1024,
                    "r8brain padding failed");
            allL.insert(allL.end(), outL.begin(), outL.begin() + block.produced_frames);
            allR.insert(allR.end(), outR.begin(), outR.begin() + block.produced_frames);
            paddingInputFrames += 1024;
            require(paddingInputFrames <= 8192, "unexpected r8brain tail length");
        }
        allL.resize(10368000); allR.resize(10368000);
        require(allL.size() == allR.size(), "channel frame counts differ");
        std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
        require(bool(output), "cannot open output");
        const uint64_t frames = allL.size();
        output.write(reinterpret_cast<const char*>(&frames), sizeof(frames));
        output.write(reinterpret_cast<const char*>(allL.data()), frames * sizeof(double));
        output.write(reinterpret_cast<const char*>(allR.data()), frames * sizeof(double));
        require(bool(output), "output write failed");
        std::cout << "{\"inputFrames\":5184000,\"outputFrames\":" << frames
                  << ",\"paddingInputFrames\":" << paddingInputFrames
                  << ",\"sourceRate\":96000,\"outputRate\":192000,\"channels\":2}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
