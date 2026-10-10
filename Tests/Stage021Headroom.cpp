// Bounded OFFLINE integration regression. No source subscriber or DAC owner.
#include "Engine/Control/GraphControlAccess.h"
#include "Engine/Control/ProfileStore.h"
#include "Engine/Egress/LiveSinkWorkers.h"
#include <entropy/pcm24/Pcm24ModuleEngine.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace entropy;
using J = ipc::Value;
using A = engine::GraphControlAccess;
constexpr unsigned frames = 256;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void save(const std::filesystem::path& path, const J& value) {
    std::ofstream file(path); file << ipc::dump(value); check(bool(file), "report write");
}

struct Fixture {
    engine::AudioGraph graph;
    engine::PlanarBuffer input, output;
    Fixture(unsigned program, unsigned mode) {
        check(graph.prepare(frames), "Graph prepare");
        graph.enableOriginalMusicAnalysis(true);
        auto parameter = [&](const std::string& name, double v) {
            check(graph.setOfflineParameter(name.c_str(), v), "offline parameter");
        };
        parameter("cinemaSource", program);
        parameter("bassExtractor", 0);
        for (unsigned ch=1; ch<=16; ++ch) {
            const auto n=std::to_string(ch);
            const double role=ch==1 ? 1 : ch==2 ? 6 : ch==3 ? 11 : 0;
            parameter("source"+n, role); parameter("cinemaRole"+n, role);
            for (const char* field : {"hpOn", "lpOn", "allPass", "protectorOn", "processMode", "musicGainDb", "cinemaGainDb", "musicDelayMs", "cinemaDelayMs"})
                parameter(std::string(field)+n, 0);
        }
        for (unsigned ch=1; ch<=3; ++ch) parameter("musicTrimDb"+std::to_string(ch), 0);
        for (unsigned ch=1; ch<=10; ++ch) parameter("cinemaTrimDb"+std::to_string(ch), 0);
        check(graph.applyOfflineParameters(true), "quiescent settings");
        EntropyOutputMatrixState routing{}; routing.struct_size=sizeof(routing);
        routing.generation=17; routing.active_source_mask=routing.enabled_destination_mask=7;
        for (unsigned c=0; c<3; ++c) routing.source_destination_mask[c]=1u<<c;
        check(A::matrix(graph).setState(routing)==ENTROPY_OK, "same routing");
        EntropyHeadroomState ledger{}; ledger.struct_size=sizeof(ledger); ledger.revision=23;
        ledger.group_count=1; ledger.active_channel_mask=7; ledger.transition_ms=1;
        ledger.groups[0].struct_size=sizeof(ledger.groups[0]); ledger.groups[0].bounded=1;
        ledger.groups[0].preventive_reserve_db=3;
        auto& h=A::headroom(graph);
        check(h.setState(ledger)==ENTROPY_OK && h.setManualGainDb(mode==0 ? -12 : -3)==ENTROPY_OK
              && h.setMode(mode)==ENTROPY_OK, "same Headroom policy");
        input.prepare(13,frames); output.prepare(16,frames); input.clear(frames);
        for (unsigned b=0; b<40; ++b) process();
    }
    void signal(double music, double cinema) {
        input.clear(frames);
        for (unsigned f=0; f<frames; ++f) {
            const double shape=f%19==0 ? -0.0 : (f%2 ? -1.0 : 1.0);
            for (unsigned c=0; c<3; ++c) {
                const double weight=c==0 ? 1 : c==1 ? .5 : -.3;
                input.channel(c)[f]=music*shape*weight;
                input.channel(c+3)[f]=cinema*shape*weight;
            }
        }
    }
    void process() {
        check(graph.process(input.readPointers(),output.writePointers(),frames)==ENTROPY_OK, "Graph process");
        const auto& matrix=A::matrixOutput(graph);
        double peak=0;
        for (unsigned c=0; c<3; ++c) for (unsigned f=0; f<frames; ++f)
            peak=std::max(peak,std::abs(matrix.readChannel(c)[f]));
        const auto telemetry=A::headroom(graph).telemetry();
        check(telemetry.measured_input_peak==peak, "current post-Matrix peak missing/stale in selected program");
        check(telemetry.applied_revision==A::headroom(graph).state().revision, "Headroom revision incoherent");
        check(!telemetry.fault_flags && !telemetry.true_peak_status && telemetry.measured_input_true_peak==0, "fault/false true-peak claim");
    }
};

J policyRegression() {
    J::Array modes;
    for (unsigned mode=0; mode<3; ++mode) {
        auto music=std::make_unique<Fixture>(0,mode), cinema=std::make_unique<Fixture>(1,mode);
        double outputError=0, decisionError=0, peakError=0, multiplierError=0, maximumPeak=0;
        unsigned identityChecks=0;
        const double scalar=mode==1 ? 1 : std::pow(10.0,-3.0/20.0);
        const std::array<double,8> sequence{.1,.9,2.0,.3,.4,1.4,.2,.01};
        for (unsigned b=0; b<32; ++b) {
            const double amplitude=sequence[b%sequence.size()];
            music->signal(amplitude,amplitude); cinema->signal(amplitude,amplitude);
            music->process(); cinema->process();
            const auto m=A::headroom(music->graph).telemetry(), c=A::headroom(cinema->graph).telemetry();
            peakError=std::max(peakError,std::abs(m.measured_input_peak-c.measured_input_peak));
            decisionError=std::max({decisionError,std::abs(m.groups[0].current_gain-c.groups[0].current_gain),
                                    std::abs(m.groups[0].target_gain-c.groups[0].target_gain),
                                    std::abs(m.auto_analysis_reduction_db-c.auto_analysis_reduction_db)});
            maximumPeak=std::max(maximumPeak,m.measured_input_peak);
            for (unsigned lane=0; lane<3; ++lane) for (unsigned f=0; f<frames; ++f) {
                check(A::matrixOutput(music->graph).readChannel(lane)[f]==A::matrixOutput(cinema->graph).readChannel(lane)[f], "fixture post-Matrix inputs differ");
                const double a=music->output.readChannel(lane)[f], z=cinema->output.readChannel(lane)[f];
                outputError=std::max(outputError,std::abs(a-z));
                if (mode) {
                    const auto& x=A::matrixOutput(music->graph).readChannel(lane)[f];
                    multiplierError=std::max(multiplierError,std::abs(a-x*scalar));
                    if (mode==1) { check(std::memcmp(&a,&x,sizeof(double))==0, "BIT PERFECT changed payload/sign-zero"); ++identityChecks; }
                }
            }
        }
        check(maximumPeak>1.5 && outputError==0 && decisionError==0 && peakError==0 && multiplierError==0, "program asymmetry or fixed-mode multiplier changed");
        modes.emplace_back(J::Object{{"mode",mode},{"blocks",32},{"maximumPostMatrixPeak",maximumPeak},
            {"outputMaximumAbsoluteError",outputError},{"decisionMaximumAbsoluteError",decisionError},
            {"peakMaximumAbsoluteError",peakError},{"fixedMultiplierMaximumAbsoluteError",multiplierError},
            {"bitExactChecks",identityChecks}});
    }
    // One switch with an existing AUTO gain tail. The detector checks the actual
    // emitted crossfade block, never an assumed source or cached MUSIC peak.
    Fixture switched(0,0); switched.signal(2, .05); switched.process();
    const double before=A::headroom(switched.graph).telemetry().groups[0].current_gain;
    check(before<.51, "AUTO safety response absent");
    check(switched.graph.setOfflineParameter("cinemaSource",1) && switched.graph.applyOfflineParameters(false), "switch prepare");
    unsigned transitionEvents=0, mixedBlocks=0; double after=1;
    for (unsigned b=0; b<60; ++b) {
        switched.signal(.05, .3); switched.process();
        transitionEvents+=A::crossover(switched.graph)->programBlockState().transitions!=0;
        const double emitted=std::abs(A::matrixOutput(switched.graph).readChannel(0)[2]);
        mixedBlocks+=emitted>.05 && emitted<.3;
        after=A::headroom(switched.graph).telemetry().groups[0].current_gain;
        check(after<1, "program switch reset/forgot AUTO hold tail");
    }
    check(A::crossover(switched.graph)->programBlockState().program==1 && transitionEvents==1 && mixedBlocks>0, "switch/crossfade not exercised");
    switched.signal(.05,1.4); switched.process();
    return J::Object{{"status","PASS"},{"sameSamplePrograms",modes},{"switches",1},
        {"transitionEvents",transitionEvents},{"mixedCrossfadeBlocks",mixedBlocks},{"gainBeforeSwitch",before},{"gainAfterHoldTail",after},
        {"detectorInput","actual post-Matrix/pre-gain block including crossfade"},{"truePeak","NOT IMPLEMENTED"}};
}

J pcmReplay(const std::filesystem::path& profilePath,const std::filesystem::path& taps) {
    control::ProfileStore store;const auto profile=store.load(profilePath);
    const auto domains=control::LiveSinkWorkers::decode(profile.document.at("stage05_devices").at("configuration"));
    J::Array results;uint64_t totalOverrange=0;
    for (unsigned d=0; d<2; ++d) if(domains[d].enabled) {
        const auto& domain=domains[d];const auto path=taps/("post-asrc-"+std::to_string(d)+".f64");
        std::ifstream raw(path,std::ios::binary);check(bool(raw),"existing post-ASRC tap missing");
        pcm24::Pcm24ModuleEngine pcm;EntropyPcm24Config config{};config.struct_size=sizeof(config);
        config.channels=domain.channels;config.maximum_frames=1024;config.output_format=domain.format;
        config.reserved_u32=ENTROPY_PCM24_OVERRANGE_CLIP_AND_REPORT;config.dither_mode=ENTROPY_PCM24_DITHER_TPDF_FLAT;
        config.seed_mode=ENTROPY_PCM24_SEED_DETERMINISTIC_TEST;config.seed=0x12345678;check(pcm.configure(config),"unchanged PCM24 test configuration");
        const auto before=pcm.telemetry();std::vector<double> block(size_t(domain.channels)*1024);
        std::vector<uint8_t> packed(size_t(domain.channels)*1024*pcm.bytesPerSample());
        std::array<const double*,16> inputs{};for(unsigned c=0;c<domain.channels;++c)inputs[c]=block.data()+c*1024;
        std::array<uint64_t,16> overrange{};std::array<double,16> peak{};uint64_t blocks=0;
        J first;
        while(raw.read(reinterpret_cast<char*>(block.data()),std::streamsize(block.size()*sizeof(double)))) {
            for(unsigned f=0;f<1024;++f)for(unsigned c=0;c<domain.channels;++c){const double v=inputs[c][f];
                check(std::isfinite(v),"nonfinite historical tap");peak[c]=std::max(peak[c],std::abs(v));
                if(std::abs(v)>1){++overrange[c];if(first.isNull())first=J::Object{{"tap","post-ASRC/pre-PCM"},{"dac",d},{"physicalPin",c+1},{"matrixDestination",domain.lanes[c]+1},{"renderFrame",J::u64(blocks*1024+f)},{"value",v}};}}
            EntropyPcm24BlockF64 b{};b.struct_size=sizeof(b);b.channels=domain.channels;b.frames=1024;b.inputs=inputs.data();
            b.output_interleaved=packed.data();b.output_capacity_bytes=unsigned(packed.size());
            check(pcm.process(b)==ENTROPY_OK,"historical PCM replay");++blocks;
        }
        check(raw.eof() && raw.gcount()==0 && blocks==1117,"historical tap truncated/size mismatch");
        const auto after=pcm.telemetry();J::Array pins;uint64_t sum=0;
        for(unsigned c=0;c<domain.channels;++c){sum+=overrange[c];pins.emplace_back(J::Object{{"physicalPin",c+1},{"matrixDestination",domain.lanes[c]+1},{"samplePeak",peak[c]},{"overrangeSamples",J::u64(overrange[c])}});}
        check(sum==after.overrange_sample_count-before.overrange_sample_count,"PCM counter delta differs from FP64 domain");
        totalOverrange+=sum;
        results.emplace_back(J::Object{{"dac",d},{"blocks",J::u64(blocks)},{"overrangeDelta",J::u64(sum)},
            {"railClampDelta",J::u64(after.reserved[0]-before.reserved[0])},{"clippedBlocksDelta",J::u64(after.reserved[2]-before.reserved[2])},
            {"nonfiniteDelta",J::u64(after.nonfinite_sample_count-before.nonfinite_sample_count)},{"firstOverrange",first},{"pins",pins}});
    }
    check(results.size()==2,"both recorded DAC domains must be checked");
    return J::Object{{"counterRegression","PASS"},{"rangeStatus",totalOverrange ? "FAIL OBSERVED OVERRANGE" : "PASS NO OBSERVED OVERRANGE"},
        {"totalOverrangeDelta",J::u64(totalOverrange)},{"scope","historical Stage020 fragment/active MANUAL -3dB; not current live counters"},
        {"profileMode",profile.parameters.mode},{"profileManualControlDb",profile.parameters.manualGainDb},{"domains",results},
        {"seed","deterministic offline TPDF; production seed/integration unchanged"},{"hardwareOpened",false}};
}

int main(int argc,char** argv) try {
    check(argc==2||argc==4,"Usage: EntropyStage021Headroom output-directory [Stage020-profile-copy Stage020-tap-root]");
    const auto root=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(root);
    const auto policy=policyRegression();save(root/"headroom-program.json",policy);
    if(argc==4)save(root/"pcm-deltas.json",pcmReplay(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3])));
    std::cout<<"PASS MUSIC/CINEMA AUTO same samples/decision; MANUAL exact scalar; BIT PERFECT exact payload; one switch with gain tail\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL Stage021 "<<e.what()<<'\n';return 1; }
