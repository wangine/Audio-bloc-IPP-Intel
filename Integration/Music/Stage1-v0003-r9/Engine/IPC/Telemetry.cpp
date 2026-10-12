#include "Telemetry.h"
#include <fstream>
namespace entropy::music {
void writeDiagnostic(const std::filesystem::path &path, const Stage1ControlResponse &r) {
    std::ofstream file(path);
    file << "{\n\"input_channels\":2,\"output_channels\":2,\"sample_rate\":192000,\"frames\":1024,\n\"blocks_"
            "published\":"
         << r.blocksPublished << ",\"publisher_drops\":" << r.publisherDrops
         << ",\"clock_qpc_frequency\":" << r.clockQpcFrequency
         << ",\"clock_late_blocks\":" << r.clockLateBlocks << ",\"sources\":[";
    for (unsigned i = 0; i < 2; i++) {
        auto &s = r.sources[i];
        if (i)
            file << ',';
        file << "{\"state\":" << s.state << ",\"rate\":" << s.rate
             << ",\"original_container_bits\":" << s.containerBits
             << ",\"original_valid_bits\":" << s.validBits
             << ",\"payload_representation\":" << s.representation
             << ",\"metadata_flags\":" << s.metadataFlags << ",\"mode\":" << s.mode
             << ",\"generation\":" << s.generation << ",\"packet_sequence\":" << s.packetSequence
             << ",\"first_source_frame\":" << s.firstSourceFrame << ",\"qpc_timestamp\":" << s.qpcTimestamp
             << ",\"qpc_frequency\":" << s.qpcFrequency << ",\"packets\":" << s.packets
             << ",\"frames_in\":" << s.framesIn << ",\"frames_out\":" << s.framesOut
             << ",\"lost_frames\":" << s.lostFrames << ",\"overruns\":" << s.overruns
             << ",\"underruns\":" << s.underruns << ",\"faults\":" << s.faults
             << ",\"sample_peak_L\":" << s.prePeakL << ",\"sample_peak_R\":" << s.prePeakR
             << ",\"true_peak_estimate_L\":" << s.truePeakL << ",\"true_peak_estimate_R\":" << s.truePeakR
             << ",\"true_peak_ready\":" << s.truePeakReady << ",\"src_startup_peak_L\":" << s.startupPeakL
             << ",\"src_startup_peak_R\":" << s.startupPeakR
             << ",\"startup_frames_measured\":" << s.startupFramesMeasured
             << ",\"startup_complete\":" << s.startupComplete << "}";
    }
    file << "]\n}\n";
}
} // namespace entropy::music
