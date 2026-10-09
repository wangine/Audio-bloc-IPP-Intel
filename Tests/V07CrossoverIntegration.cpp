// Offline differential oracle: native Crossover versus the published V07 DLL.
// A wrong role/index, PCA port, filter, delay or program transition must fail.
// No DAC, StageBus peer, device discovery, profile mutation or production IPC.
#include "PluginProcessor.h"
#include "IPC/Json.h"
#include <entropy/crossover_abi.h>
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Native = entropy::standalone::EntropyCrossoverAudioProcessor;
constexpr unsigned frames = 1024;
constexpr double rate = 192000, pi = std::numbers::pi_v<double>;
constexpr const char* expectedHash = "1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A";
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }

std::string sha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "cannot read V07 DLL for SHA256");
    struct Hash {
        BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
        std::vector<unsigned char> object;
        ~Hash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } state;
    require(BCryptOpenAlgorithmProvider(&state.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "SHA256 provider");
    DWORD objectSize = 0, received = 0;
    require(BCryptGetProperty(state.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &received, 0) >= 0, "SHA256 object size");
    state.object.resize(objectSize);
    require(BCryptCreateHash(state.algorithm, &state.hash, state.object.data(), objectSize, nullptr, 0, 0) >= 0, "SHA256 create");
    std::array<unsigned char, 65536> chunk{};
    while (input) {
        input.read(reinterpret_cast<char*>(chunk.data()), chunk.size());
        auto count = input.gcount();
        if (count) require(BCryptHashData(state.hash, chunk.data(), static_cast<ULONG>(count), 0) >= 0, "SHA256 data");
    }
    require(input.eof(), "SHA256 read failed");
    std::array<unsigned char, 32> digest{};
    require(BCryptFinishHash(state.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0, "SHA256 finish");
    std::ostringstream result; result << std::hex << std::uppercase << std::setfill('0');
    for (auto byte : digest) result << std::setw(2) << unsigned(byte);
    return result.str();
}

// Read only the Crossover payload from the copied project; never instantiate a host.
std::vector<unsigned char> profileState(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "cannot read copied project");
    std::string text((std::istreambuf_iterator<char>(file)), {});
    require(text.size() <= 32 * 1024 * 1024, "copied project oversized");
    auto project = entropy::ipc::parse(text);
    require(project.at("format").asString() == "EntropyProject" && project.at("host_engine_rate").asUnsigned() == 192000, "copied project contract");
    std::vector<unsigned char> result; unsigned matches = 0;
    for (const auto& module : project.at("modules").asArray()) {
        if (module.at("instance_id").asString() != "crossover.main") continue;
        require(++matches == 1 && module.at("module_id").asString() == "audio.entropy.crossover" && module.at("encoding").asString() == "base64", "copied Crossover identity");
        const auto& encoded = module.at("serialized_state").asString(); DWORD size = 0;
        require(CryptStringToBinaryA(encoded.c_str(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr), "copied Crossover base64 size");
        result.resize(size);
        require(CryptStringToBinaryA(encoded.c_str(), static_cast<DWORD>(encoded.size()), CRYPT_STRING_BASE64, result.data(), &size, nullptr, nullptr), "copied Crossover base64");
        uint64_t checksum = 14695981039346656037ULL;
        for (auto byte : result) checksum = (checksum ^ byte) * 1099511628211ULL;
        std::ostringstream hex; hex << std::hex << std::setw(16) << std::setfill('0') << checksum;
        require(hex.str() == module.at("state_checksum_fnv1a64").asString(), "copied Crossover checksum");
    }
    require(matches == 1 && !result.empty(), "copied project missing Crossover");
    return result;
}

struct Golden {
    HMODULE library = nullptr; EntropyModuleApiV1 api{}; EntropyCrossoverApiV1 control{}; EntropyModuleHandle handle = nullptr;
    explicit Golden(const std::filesystem::path& path) {
        library = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        require(library != nullptr, "V07 LoadLibrary error " + std::to_string(GetLastError()));
        auto get = reinterpret_cast<EntropyModuleGetApiFn>(GetProcAddress(library, ENTROPY_MODULE_GET_API_SYMBOL));
        api.struct_size = sizeof(api);
        require(get && get(ENTROPY_MODULE_ABI_VERSION, &api) == ENTROPY_OK, "V07 module ABI");
        control.struct_size = sizeof(control);
        require(api.query_extension(ENTROPY_CROSSOVER_EXTENSION_ID, ENTROPY_CROSSOVER_EXTENSION_VERSION, &control, sizeof(control)) == ENTROPY_OK, "V07 Crossover ABI");
        EntropyHostServices host{}; host.struct_size = sizeof(host); host.abi_version = ENTROPY_MODULE_ABI_VERSION;
        require(api.create(&host, &handle) == ENTROPY_OK, "V07 create");
    }
    ~Golden() { if (handle) api.destroy(handle); if (library) FreeLibrary(library); }
    void prepare() {
        EntropyPrepareInfo info{}; info.struct_size = sizeof(info); info.sample_rate = 192000;
        info.maximum_frames = frames; info.input_channels = 13; info.output_channels = 16;
        require(api.prepare(handle, &info) == ENTROPY_OK, "V07 prepare");
    }
    void set(const std::string& id, double value) { require(control.set_parameter(handle, id.c_str(), value) == ENTROPY_OK, "V07 set " + id); }
    double get(const std::string& id) {
        double value = 0; require(control.get_parameter(handle, id.c_str(), &value) == ENTROPY_OK, "V07 get " + id); return value;
    }
};

struct Metric {
    uint64_t count = 0, nonfinite = 0, bits = 0; double maximum = 0, error2 = 0, reference2 = 0, peak = 0, referencePeak = 0;
    int64_t first = -1, referenceFirst = -1, peakAt = -1, referencePeakAt = -1;
    void add(double native, double reference, uint64_t at) {
        if (!std::isfinite(native) || !std::isfinite(reference)) { ++nonfinite; return; }
        ++count; bits += std::bit_cast<uint64_t>(native) != std::bit_cast<uint64_t>(reference);
        maximum = std::max(maximum, std::abs(native - reference)); error2 += (native-reference)*(native-reference); reference2 += reference*reference;
        if (std::abs(native) > peak) { peak = std::abs(native); peakAt = static_cast<int64_t>(at); }
        if (std::abs(reference) > referencePeak) { referencePeak = std::abs(reference); referencePeakAt = static_cast<int64_t>(at); }
        if (first < 0 && std::abs(native) > 1e-10) first = static_cast<int64_t>(at);
        if (referenceFirst < 0 && std::abs(reference) > 1e-10) referenceFirst = static_cast<int64_t>(at);
    }
    double rmsError() const { return count ? std::sqrt(error2/count) : INFINITY; }
    // Same thresholds as Stage02/Validation.cpp. V07 differences are failures here.
    bool pass() const { return count && !nonfinite && maximum <= 1e-12 + 1e-11*referencePeak && rmsError() <= 1e-13 + 1e-12*std::sqrt(reference2/count); }
};

struct Pair {
    std::unique_ptr<Native> native = std::make_unique<Native>(); Golden golden;
    std::array<std::array<double, frames>, 16> input{}, output{}, reference{}, storage{};
    std::array<const double*,16> inputs{}; std::array<double*,16> outputs{}, references{}, working{};
    unsigned parameterFailures = 0;
    explicit Pair(const std::filesystem::path& dll) : golden(dll) {
        for (unsigned c=0;c<16;++c) { inputs[c]=input[c].data(); outputs[c]=output[c].data(); references[c]=reference[c].data(); working[c]=storage[c].data(); }
        reset();
    }
    void set(const std::string& id, double value) {
        auto* parameter = native->getAPVTS().getParameter(id.c_str()); require(parameter != nullptr, "native set " + id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
        golden.set(id, value);
    }
    void flush() { native->serviceUpdates(); }
    void program(unsigned value) { set("cinemaSource", value); flush(); }
    void normalize() {
        for (auto* id : {"globalDelayMs", "musicGlobalDelayMs", "cinemaGlobalDelayMs"}) golden.set(id, 0);
        for (unsigned c=1;c<=16;++c) {
            auto* parameter = native->getAPVTS().getParameter(("fractionalDelayMode"+std::to_string(c)).c_str());
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0));
        }
        flush();
    }
    void reset() { native->prepareToPlay(rate, frames); golden.prepare(); }
    void imported(const std::vector<unsigned char>& bytes) {
        require(native->restoreStateInformation(bytes.data(), static_cast<int>(bytes.size())), "native copied profile import");
        require(golden.control.import_state(golden.handle, bytes.data(), static_cast<uint32_t>(bytes.size())) == ENTROPY_OK, "V07 copied profile import");
        reset(); normalize();
    }
    void compareParameters(std::ostream& csv, const std::string& name) {
        for (const auto& descriptor : Native::createParameterLayout()) {
            const auto id = descriptor->paramID.toStdString();
            if (id.starts_with("fractionalDelayMode")) continue;
            const auto x = native->getAPVTS().getRawParameterValue(id.c_str())->load(); const double y = golden.get(id);
            // Parameters are FP32 in both implementations; compare exact accepted scalars.
            const bool ok = std::isfinite(x) && std::isfinite(y) && double(x) == y;
            parameterFailures += !ok;
            csv << name << ',' << id << ',' << x << ',' << y << ',' << (ok?"PASS":"FAIL") << '\n';
        }
    }
    void process(uint64_t at) {
        for (unsigned c=0;c<16;++c) { if (c<13) storage[c]=input[c]; else storage[c].fill(0); }
        entropy::native::AudioBuffer<double> view(working.data(), 16, frames); entropy::native::MidiBuffer midi;
        native->processBlock(view, midi);
        for (unsigned c=0;c<16;++c) output[c]=storage[c];
        EntropyAudioBlockF64 block{}; block.struct_size=sizeof(block); block.frames=frames; block.input_channels=13; block.output_channels=16;
        block.inputs=inputs.data(); block.outputs=references.data(); EntropyProcessContext context{}; context.struct_size=sizeof(context); context.stream_time_samples=at;
        require(golden.api.process_f64(golden.handle, &block, &context) == ENTROPY_OK, "V07 process");
    }
};

struct Filter { const char* name; int mode, type, length; bool enabled; };
constexpr Filter filters[] = {{"bypass",0,2,0,false},{"LR24",0,2,0,true},{"LR48",0,3,0,true},{"FIR2049",1,2,0,true},{"FIR8193",1,2,2,true}};
void configure(Pair& pair, unsigned program, unsigned extractor, const Filter& filter) {
    pair.program(program); pair.normalize();
    for (auto [id,value] : {std::pair{"lrWays",5.}, {"centerWays",2.}, {"subSpeakers",2.}, {"scoopSpeakers",2.}, {"bassFeed",1.}, {"scoopFeed",1.}, {"bassLinkMode",0.}, {"cinemaScoopSource",0.}, {"cinemaLfeReferenceGain",0.}, {"cinemaLfeFilterBypass",0.}}) pair.set(id,value);
    pair.set("bassExtractor",extractor); pair.set("scoopExtractor",extractor);
    for (unsigned c=1;c<=3;++c) pair.set("musicTrimDb"+std::to_string(c),0);
    for (unsigned c=1;c<=10;++c) pair.set("cinemaTrimDb"+std::to_string(c),0);
    constexpr unsigned musicRoles[] = {1,6,11,16,17,18,19};
    constexpr unsigned cinemaRoles[] = {1,6,11,16,17,18,19,20,21,22,23,24,25};
    for (unsigned c=0;c<16;++c) {
        auto channel = [&](const char* base, double value) { pair.set(std::string(base)+std::to_string(c+1),value); };
        channel("source",musicRoles[c%7]); channel("cinemaRole",cinemaRoles[c%13]);
        const unsigned role = program?cinemaRoles[c%13]:musicRoles[c%7]; const bool bass=role>=16&&role<=19;
        channel("hpOn",filter.enabled); channel("lpOn",filter.enabled); channel("hpHz",bass?20:100); channel("lpHz",bass?240:8000);
        channel("filterType",filter.type); channel("processMode",filter.mode); channel("firLength",filter.length);
        channel("allPass",0); channel("apFreq",1000); channel("apQ",1); channel("gainDb",0); channel("delayMs",0);
        channel("musicGainDb",0); channel("cinemaGainDb",0); channel("musicDelayMs",0); channel("cinemaDelayMs",0);
        channel("mute",0); channel("solo",0); channel("polarity",0); channel("protectorOn",0); channel("emergencyMute",0);
    }
    pair.flush(); pair.reset();
}

enum class Stimulus { impulse, bassTone, midTone, deterministic };
const char* stimulusName(Stimulus input) {
    switch(input) {case Stimulus::impulse:return "impulse";case Stimulus::bassTone:return "78.125Hz";case Stimulus::midTone:return "1500Hz";default:return "deterministic";}
}
void stimulus(Pair& pair, Stimulus kind, uint64_t at) {
    for (unsigned c=0;c<13;++c) for (unsigned i=0;i<frames;++i) {
        const uint64_t n=at+i; double value=0;
        if (kind==Stimulus::impulse) value=n==0?.125*(1+c*.03):0;
        else if (kind==Stimulus::bassTone || kind==Stimulus::midTone) {
            // MUSIC L/R/C and CINEMA L/R/C are coherent; LFE/surrounds distinct.
            const double frequency=kind==Stimulus::bassTone?78.125:1500;
            value=.125*std::sin(2*pi*frequency*n/rate)*(c==1||c==4?.8:c==2||c==5?.5:1);
        } else {
            uint32_t random=static_cast<uint32_t>(n+1)*747796405u+static_cast<uint32_t>(c+1)*2891336453u;
            random=(random^(random>>16))*2246822519u; random^=random>>13;
            // Correlated tones plus an opposite-polarity R and deterministic noise.
            const double polarity=c==1||c==4?-1:1;
            value=polarity*(.06*std::sin(2*pi*78.125*n/rate)+.04*std::sin(2*pi*1500*n/rate))+.01*(double(random)/4294967295.-.5);
        }
        pair.input[c][i]=value;
    }
}

unsigned writeMetrics(std::ostream& csv, const std::string& name, const std::array<Metric,16>& metrics, bool requireSignal) {
    unsigned failures=0;
    for (unsigned c=0;c<16;++c) {
        const auto& m=metrics[c]; const bool ok=m.pass()&&(!requireSignal||(m.peak>1e-10&&m.referencePeak>1e-10)); failures+=!ok;
        csv<<name<<','<<c+1<<','<<m.maximum<<','<<m.rmsError()<<','<<m.peak<<','<<m.referencePeak<<','<<m.bits<<','<<m.nonfinite<<','<<m.first<<','<<m.referenceFirst<<','<<m.peakAt<<','<<m.referencePeakAt<<','<<(ok?"PASS":"FAIL")<<'\n';
        if (!ok) std::printf("FAIL %s CH%u max=%.17g rms=%.17g nativePeak=%.17g V07Peak=%.17g\n",name.c_str(),c+1,m.maximum,m.rmsError(),m.peak,m.referencePeak);
    }
    return failures;
}
unsigned capture(Pair& pair, std::ostream& csv, const std::string& name, Stimulus kind, bool profile, bool inject) {
    pair.reset(); std::array<Metric,16> metrics{};
    const unsigned warm=kind==Stimulus::impulse?0:192*frames, count=64*frames;
    for (uint64_t at=0;at<warm+count;at+=frames) {
        stimulus(pair,kind,at); pair.process(at);
        if (inject && at==warm) pair.output[0][0]+=.001;
        if (at>=warm) for(unsigned c=0;c<16;++c)for(unsigned i=0;i<frames;++i)metrics[c].add(pair.output[c][i],pair.reference[c][i],at+i-warm);
    }
    return writeMetrics(csv,name,metrics,!profile&&(kind==Stimulus::bassTone||kind==Stimulus::deterministic));
}
unsigned transitions(Pair& pair, std::ostream& csv, std::ostream& parameters, const std::string& name) {
    pair.program(0); pair.reset(); std::array<Metric,16> metrics{};
    for (unsigned block=0;block<192;++block) {
        if(block==64||block==128) { pair.program(block==64?1:0); pair.compareParameters(parameters,name+"-program"+std::to_string(block)); }
        const uint64_t at=uint64_t(block)*frames; stimulus(pair,Stimulus::deterministic,at); pair.process(at);
        for(unsigned c=0;c<16;++c)for(unsigned i=0;i<frames;++i)metrics[c].add(pair.output[c][i],pair.reference[c][i],at+i);
    }
    return writeMetrics(csv,name,metrics,false);
}
}

int main(int argc,char** argv) try {
    require(argc>=3&&argc<=5,"usage: EntropyV07Integration verified-V07-DLL output-directory [copied-project.entropy] [--inject-error]");
    bool inject=false; std::vector<unsigned char> profile;
    for(int i=3;i<argc;++i) { if(std::string(argv[i])=="--inject-error")inject=true;else profile=profileState(std::filesystem::u8path(argv[i])); }
    const auto dll=std::filesystem::absolute(std::filesystem::u8path(argv[1]));
    const auto hash=sha256(dll); require(hash==expectedHash,"V07 SHA256 mismatch: "+hash); std::printf("V07 SHA256=%s\n",hash.c_str());
    const auto directory=std::filesystem::u8path(argv[2]); std::filesystem::create_directories(directory);
    std::ofstream csv(directory/"v07-crossover.csv"), parameters(directory/"v07-parameters.csv");
    require(bool(csv)&&bool(parameters),"cannot write oracle metrics"); csv.exceptions(std::ios::badbit|std::ios::failbit); parameters.exceptions(std::ios::badbit|std::ios::failbit);
    csv<<std::setprecision(17)<<"case,output,max_absolute_error,rms_error,peak,V07_peak,bit_mismatches,nonfinite,first_sample,V07_first_sample,peak_sample,V07_peak_sample,status\n";
    parameters<<std::setprecision(17)<<"case,parameter,native_value,V07_value,status\n";
    // Prove the acceptance gate rejects a finite corrupted signal and nonfinite output.
    Metric corrupt; corrupt.add(.126,.125,0); require(!corrupt.pass(),"oracle failed to reject sample corruption");
    Metric nonfinite; nonfinite.add(INFINITY,0,0); require(!nonfinite.pass(),"oracle failed to reject nonfinite output");
    unsigned failures=0,cases=0;
    for(unsigned program=0;program<2;++program)for(unsigned extractor:{1u,3u})for(const auto& filter:filters) {
        auto pair=std::make_unique<Pair>(dll); configure(*pair,program,extractor,filter);
        const std::string name=std::string(program?"CINEMA":"MUSIC")+"-extractor"+std::to_string(extractor)+"-"+filter.name;
        pair->compareParameters(parameters,name);
        for(auto kind:{Stimulus::impulse,Stimulus::bassTone,Stimulus::midTone,Stimulus::deterministic}) {
            failures+=capture(*pair,csv,name+"-"+stimulusName(kind),kind,false,inject); ++cases;
            if(inject) { csv.flush(); parameters.flush(); std::printf("Injected-error oracle failures=%u\n",failures); return failures?1:0; }
        }
        failures+=pair->parameterFailures;
        std::printf("V07 CASE %s failures=%u\n",name.c_str(),failures); std::fflush(stdout);
    }
    for(const auto& filter:{filters[1],filters[3]}) {
        auto pair=std::make_unique<Pair>(dll);
        // Populate both saved scenes before the uninterrupted 0->1->0 stream.
        configure(*pair,0,3,filter); configure(*pair,1,3,filter);
        failures+=transitions(*pair,csv,parameters,std::string("program-transition-")+filter.name); failures+=pair->parameterFailures; ++cases;
    }
    if(!profile.empty()) {
        for(unsigned program=0;program<2;++program) {
            auto pair=std::make_unique<Pair>(dll); pair->imported(profile); pair->program(program); pair->normalize(); pair->reset();
            const std::string name=std::string("copied-profile-")+(program?"CINEMA":"MUSIC"); pair->compareParameters(parameters,name);
            for(auto kind:{Stimulus::impulse,Stimulus::bassTone,Stimulus::midTone,Stimulus::deterministic}) { failures+=capture(*pair,csv,name+"-"+stimulusName(kind),kind,true,false); ++cases; }
            failures+=pair->parameterFailures;
        }
    }
    csv.flush(); parameters.flush(); std::printf("V07 STRICT RESULT cases=%u failures=%u copied_profile=%s\n",cases,failures,profile.empty()?"NOT_SUPPLIED":"EXECUTED"); return failures?1:0;
} catch(const std::exception& error) { std::fprintf(stderr,"V07 STRICT ERROR: %s\n",error.what()); return 2; }
