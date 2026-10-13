#include "Core/EngineTelemetry.h"
#include "ParametricEqEngine.h"
#include <cstring>
#include <cstdio>
#include <limits>
namespace entropy::parametric_eq {
StereoSnapshot makeDefaultStereoSnapshot() noexcept
{
    StereoSnapshot result;
    constexpr std::array<double, bandCount> frequencies{
        35.0, 80.0, 180.0, 420.0, 1100.0, 2800.0, 6500.0, 14000.0};
    for (std::size_t index = 0; index < bandCount; ++index)
        result.bands[index].frequencyHz = frequencies[index];
    return result;
}
}
namespace entropy::host {
namespace {using namespace entropy::parametric_eq;
template<size_t N>void text(char(&o)[N],const char* i){std::snprintf(o,N,"%s",i);}
EntropyParametricEqSettingsV1 defaultSettings() noexcept
{
    const auto source = makeDefaultStereoSnapshot();
    EntropyParametricEqSettingsV1 result{};
    result.struct_size = sizeof(result);
    result.mode = ENTROPY_PARAMETRIC_EQ_MODE_BYPASS;
    result.band_count = ENTROPY_PARAMETRIC_EQ_BAND_COUNT;
    for (std::size_t index = 0; index < bandCount; ++index)
    {
        const auto& input = source.bands[index];
        auto& output = result.bands[index];
        output.struct_size = sizeof(output);
        output.enabled = input.enabled ? 1u : 0u;
        output.filter_type = static_cast<std::uint32_t>(input.type);
        output.detector_mode = static_cast<std::uint32_t>(input.detectorMode);
        output.detector_source = static_cast<std::uint32_t>(input.detectorSource);
        output.dynamics_mode = static_cast<std::uint32_t>(input.dynamicsMode);
        output.frequency_hz = input.frequencyHz;
        output.q = input.q;
        output.static_gain_db = input.staticGainDb;
        output.threshold_db = input.thresholdDb;
        output.ratio = input.ratio;
        output.knee_db = input.kneeDb;
        output.attack_ms = input.attackMs;
        output.release_ms = input.releaseMs;
        output.max_cut_db = input.maxCutDb;
        output.max_boost_db = input.maxBoostDb;
    }
    return result;
}
constexpr uint32_t parameterFieldsPerBand=15,parameterDescriptorCount=121;
EntropyResult ENTROPY_ABI_CALL parameterDescriptor(
    EntropyModuleHandle handle, std::uint32_t index,
    EntropyParametricEqParameterV1* output)
{
    if (!output || output->struct_size < sizeof(*output) ||
        index >= parameterDescriptorCount)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto defaults = defaultSettings();
    *output = {};
    output->struct_size = sizeof(*output);
    output->index = index;
    output->band_index = (std::numeric_limits<std::uint32_t>::max)();
    if (index == 0)
    {
        output->kind = ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE;
        output->choice_count = 4;
        output->field = ENTROPY_PARAMETRIC_EQ_FIELD_MODE;
        output->minimum = 0.0;
        output->maximum = 3.0;
        output->step = 1.0;
        output->default_value = ENTROPY_PARAMETRIC_EQ_MODE_BYPASS;
        text(output->identifier, "mode");
        text(output->label, "Module mode");
        return ENTROPY_OK;
    }

    const auto local = index - 1;
    const auto bandIndex = local / parameterFieldsPerBand;
    const auto fieldOffset = local % parameterFieldsPerBand;
    output->band_index = bandIndex;
    const auto& band = defaults.bands[bandIndex];
    const char* suffix = nullptr;
    const char* label = nullptr;
    switch (fieldOffset)
    {
        case 0: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_ENABLED; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_BOOLEAN; output->choice_count=2; output->minimum=0; output->maximum=1; output->step=1; output->default_value=band.enabled; suffix="enabled"; label="Enabled"; break;
        case 1: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_FILTER_TYPE; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE; output->choice_count=3; output->minimum=0; output->maximum=2; output->step=1; output->default_value=band.filter_type; suffix="filterType"; label="Filter type"; break;
        case 2: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_MODE; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE; output->choice_count=3; output->minimum=0; output->maximum=2; output->step=1; output->default_value=band.detector_mode; suffix="detectorMode"; label="Detector mode"; break;
        case 3: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_DETECTOR_SOURCE; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE; output->choice_count=3; output->minimum=0; output->maximum=2; output->step=1; output->default_value=band.detector_source; suffix="detectorSource"; label="Detector source"; break;
        case 4: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_DYNAMICS_MODE; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_CHOICE; output->choice_count=4; output->minimum=0; output->maximum=3; output->step=1; output->default_value=band.dynamics_mode; suffix="dynamicsMode"; label="Dynamics mode"; break;
        case 5: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_FREQUENCY_HZ; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=20; output->maximum=20000; output->step=1; output->default_value=band.frequency_hz; suffix="frequencyHz"; label="Frequency (Hz)"; break;
        case 6: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_Q; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=0.1; output->maximum=20; output->step=0.01; output->default_value=band.q; suffix="q"; label="Q"; break;
        case 7: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_STATIC_GAIN_DB; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=-18; output->maximum=18; output->step=0.1; output->default_value=band.static_gain_db; suffix="staticGainDb"; label="Static gain (dB)"; break;
        case 8: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_THRESHOLD_DB; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=-72; output->maximum=0; output->step=0.1; output->default_value=band.threshold_db; suffix="thresholdDb"; label="Threshold (dB)"; break;
        case 9: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_RATIO; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=1; output->maximum=20; output->step=0.01; output->default_value=band.ratio; suffix="ratio"; label="Ratio"; break;
        case 10: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_KNEE_DB; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=0; output->maximum=24; output->step=0.1; output->default_value=band.knee_db; suffix="kneeDb"; label="Knee (dB)"; break;
        case 11: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_ATTACK_MS; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=0.1; output->maximum=250; output->step=0.1; output->default_value=band.attack_ms; suffix="attackMs"; label="Attack (ms)"; break;
        case 12: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_RELEASE_MS; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=5; output->maximum=2000; output->step=1; output->default_value=band.release_ms; suffix="releaseMs"; label="Release (ms)"; break;
        case 13: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_MAX_CUT_DB; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=-24; output->maximum=0; output->step=0.1; output->default_value=band.max_cut_db; suffix="maxCutDb"; label="Maximum cut (dB)"; break;
        default: output->field=ENTROPY_PARAMETRIC_EQ_FIELD_MAX_BOOST_DB; output->kind=ENTROPY_PARAMETRIC_EQ_PARAMETER_FLOAT; output->minimum=0; output->maximum=18; output->step=0.1; output->default_value=band.max_boost_db; suffix="maxBoostDb"; label="Maximum boost (dB)"; break;
    }
    std::snprintf(output->identifier, sizeof(output->identifier),
                  "band%u.%s", bandIndex, suffix);
    std::snprintf(output->label, sizeof(output->label),
                  "Band %u %s", bandIndex + 1, label);
    return ENTROPY_OK;
}
}
void populateStage2UiSchema(EngineTelemetrySnapshot& s){
s.loudnessAvailable=1;s.loudnessParameterCount=47;s.loudnessSettings.struct_size=sizeof(s.loudnessSettings);s.loudnessSettings.parameter_count=s.loudnessParameterCount;s.loudnessTelemetry.master_bypass=1;
{auto& d=s.loudnessParameters[0];d.struct_size=sizeof(d);d.index=0;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=140;d.step=0.1;d.default_value=100;text(d.identifier,"calibration");text(d.label,"Mic calibration dB");s.loudnessSettings.values[0]=100;}
{auto& d=s.loudnessParameters[1];d.struct_size=sizeof(d);d.index=1;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=35;d.maximum=80;d.step=0.1;d.default_value=50;text(d.identifier,"lowSpl");text(d.label,"Low SPL");s.loudnessSettings.values[1]=50;}
{auto& d=s.loudnessParameters[2];d.struct_size=sizeof(d);d.index=2;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=50;d.maximum=110;d.step=0.1;d.default_value=80;text(d.identifier,"highSpl");text(d.label,"High SPL");s.loudnessSettings.values[2]=80;}
{auto& d=s.loudnessParameters[3];d.struct_size=sizeof(d);d.index=3;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=1;d.step=0.001;d.default_value=0;text(d.identifier,"minLoud");text(d.label,"Min loudness");s.loudnessSettings.values[3]=0;}
{auto& d=s.loudnessParameters[4];d.struct_size=sizeof(d);d.index=4;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=1;d.step=0.001;d.default_value=1;text(d.identifier,"maxLoud");text(d.label,"Max loudness");s.loudnessSettings.values[4]=1;}
{auto& d=s.loudnessParameters[5];d.struct_size=sizeof(d);d.index=5;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.05;d.maximum=10;d.step=0.05;d.default_value=1.5;text(d.identifier,"splAttack");text(d.label,"SPL attack");s.loudnessSettings.values[5]=1.5;}
{auto& d=s.loudnessParameters[6];d.struct_size=sizeof(d);d.index=6;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.05;d.maximum=20;d.step=0.05;d.default_value=5;text(d.identifier,"splRelease");text(d.label,"SPL release");s.loudnessSettings.values[6]=5;}
{auto& d=s.loudnessParameters[7];d.struct_size=sizeof(d);d.index=7;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.05;d.maximum=15;d.step=0.05;d.default_value=3;text(d.identifier,"ctrlSmooth");text(d.label,"Control smoothing");s.loudnessSettings.values[7]=3;}
{auto& d=s.loudnessParameters[8];d.struct_size=sizeof(d);d.index=8;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=3;d.step=0.05;d.default_value=0.5;text(d.identifier,"deadband");text(d.label,"Deadband dB");s.loudnessSettings.values[8]=0.5;}
{auto& d=s.loudnessParameters[9];d.struct_size=sizeof(d);d.index=9;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=24;d.step=0.1;d.default_value=12;text(d.identifier,"maxGain");text(d.label,"Max gain dB");s.loudnessSettings.values[9]=12;}
{auto& d=s.loudnessParameters[10];d.struct_size=sizeof(d);d.index=10;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=1;d.maximum=48;d.step=0.1;d.default_value=24;text(d.identifier,"scriptScale");text(d.label,"GP script full scale");s.loudnessSettings.values[10]=24;}
{auto& d=s.loudnessParameters[11];d.struct_size=sizeof(d);d.index=11;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=1;text(d.identifier,"freshAuto");text(d.label,"Fresh Auto");s.loudnessSettings.values[11]=1;}
{auto& d=s.loudnessParameters[12];d.struct_size=sizeof(d);d.index=12;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=0;text(d.identifier,"freshBypass");text(d.label,"Fresh Bypass");s.loudnessSettings.values[12]=0;}
{auto& d=s.loudnessParameters[13];d.struct_size=sizeof(d);d.index=13;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=24;text(d.identifier,"freshMid");text(d.label,"Fresh Mid manual");s.loudnessSettings.values[13]=24;}
{auto& d=s.loudnessParameters[14];d.struct_size=sizeof(d);d.index=14;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=30;text(d.identifier,"freshHigh");text(d.label,"Fresh High manual");s.loudnessSettings.values[14]=30;}
{auto& d=s.loudnessParameters[15];d.struct_size=sizeof(d);d.index=15;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=0;text(d.identifier,"midMin");text(d.label,"Mid min");s.loudnessSettings.values[15]=0;}
{auto& d=s.loudnessParameters[16];d.struct_size=sizeof(d);d.index=16;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=45;text(d.identifier,"midMax");text(d.label,"Mid max");s.loudnessSettings.values[16]=45;}
{auto& d=s.loudnessParameters[17];d.struct_size=sizeof(d);d.index=17;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=0;text(d.identifier,"highMin");text(d.label,"High min");s.loudnessSettings.values[17]=0;}
{auto& d=s.loudnessParameters[18];d.struct_size=sizeof(d);d.index=18;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=100;d.step=0.1;d.default_value=55;text(d.identifier,"highMax");text(d.label,"High max");s.loudnessSettings.values[18]=55;}
{auto& d=s.loudnessParameters[19];d.struct_size=sizeof(d);d.index=19;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.05;d.maximum=10;d.step=0.05;d.default_value=1.5;text(d.identifier,"freshAttack");text(d.label,"Fresh attack");s.loudnessSettings.values[19]=1.5;}
{auto& d=s.loudnessParameters[20];d.struct_size=sizeof(d);d.index=20;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.05;d.maximum=20;d.step=0.05;d.default_value=4;text(d.identifier,"freshRelease");text(d.label,"Fresh release");s.loudnessSettings.values[20]=4;}
{auto& d=s.loudnessParameters[21];d.struct_size=sizeof(d);d.index=21;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-80;d.maximum=20;d.step=0.1;d.default_value=-30;text(d.identifier,"midCal");text(d.label,"Mid energy calibration");s.loudnessSettings.values[21]=-30;}
{auto& d=s.loudnessParameters[22];d.struct_size=sizeof(d);d.index=22;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-100;d.maximum=20;d.step=0.1;d.default_value=-45;text(d.identifier,"highCal");text(d.label,"High energy calibration");s.loudnessSettings.values[22]=-45;}
{auto& d=s.loudnessParameters[23];d.struct_size=sizeof(d);d.index=23;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=35;d.maximum=90;d.step=0.1;d.default_value=45;text(d.identifier,"splStart");text(d.label,"Fresh SPL start");s.loudnessSettings.values[23]=45;}
{auto& d=s.loudnessParameters[24];d.struct_size=sizeof(d);d.index=24;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=50;d.maximum=110;d.step=0.1;d.default_value=85;text(d.identifier,"splFull");text(d.label,"Fresh SPL full");s.loudnessSettings.values[24]=85;}
{auto& d=s.loudnessParameters[25];d.struct_size=sizeof(d);d.index=25;d.kind=ENTROPY_LOUDNESS_INTEGER;d.minimum=50;d.maximum=2000;d.step=1;d.default_value=200;text(d.identifier,"updateMs");text(d.label,"OSC update ms");s.loudnessSettings.values[25]=200;}
{auto& d=s.loudnessParameters[26];d.struct_size=sizeof(d);d.index=26;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=0;text(d.identifier,"oscEnable");text(d.label,"OSC Enable");s.loudnessSettings.values[26]=0;}
{auto& d=s.loudnessParameters[27];d.struct_size=sizeof(d);d.index=27;d.kind=ENTROPY_LOUDNESS_INTEGER;d.minimum=1;d.maximum=65535;d.step=1;d.default_value=54344;text(d.identifier,"oscPort");text(d.label,"OSC Port");s.loudnessSettings.values[27]=54344;}
{auto& d=s.loudnessParameters[28];d.struct_size=sizeof(d);d.index=28;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=35;d.maximum=120;d.step=0.1;d.default_value=85;text(d.identifier,"powerRefSpl");text(d.label,"Power reference SPL");s.loudnessSettings.values[28]=85;}
{auto& d=s.loudnessParameters[29];d.struct_size=sizeof(d);d.index=29;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.1;d.maximum=300;d.step=0.1;d.default_value=30;text(d.identifier,"powerRefWatts");text(d.label,"Power reference watts");s.loudnessSettings.values[29]=30;}
{auto& d=s.loudnessParameters[30];d.struct_size=sizeof(d);d.index=30;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=10;d.maximum=300;d.step=1.0;d.default_value=300;text(d.identifier,"maxPower");text(d.label,"Meter maximum watts");s.loudnessSettings.values[30]=300;}
{auto& d=s.loudnessParameters[31];d.struct_size=sizeof(d);d.index=31;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=0;text(d.identifier,"loudnessEnable");text(d.label,"Internal Loudness EQ");s.loudnessSettings.values[31]=0;}
{auto& d=s.loudnessParameters[32];d.struct_size=sizeof(d);d.index=32;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=0;text(d.identifier,"masterBypass");text(d.label,"Master bypass");s.loudnessSettings.values[32]=0;}
{auto& d=s.loudnessParameters[33];d.struct_size=sizeof(d);d.index=33;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=6.1;d.step=0.1;d.default_value=0.0;text(d.identifier,"programSumReserve");text(d.label,"Tidal/AUX sum reserve");s.loudnessSettings.values[33]=0.0;}
{auto& d=s.loudnessParameters[34];d.struct_size=sizeof(d);d.index=34;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=20;d.maximum=110;d.step=0.1;d.default_value=85;text(d.identifier,"referenceSpl");text(d.label,"Reference SPL");s.loudnessSettings.values[34]=85;}
{auto& d=s.loudnessParameters[35];d.struct_size=sizeof(d);d.index=35;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0;d.maximum=18;d.step=0.1;d.default_value=12;text(d.identifier,"maxComp");text(d.label,"Maximum compensation");s.loudnessSettings.values[35]=12;}
{auto& d=s.loudnessParameters[36];d.struct_size=sizeof(d);d.index=36;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-18;d.maximum=6;d.step=0.1;d.default_value=0;text(d.identifier,"outputTrim");text(d.label,"Output trim");s.loudnessSettings.values[36]=0;}
{auto& d=s.loudnessParameters[37];d.struct_size=sizeof(d);d.index=37;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=40;d.maximum=250;d.step=1.0;d.default_value=120;text(d.identifier,"bassFreq");text(d.label,"Bass shelf frequency");s.loudnessSettings.values[37]=120;}
{auto& d=s.loudnessParameters[38];d.struct_size=sizeof(d);d.index=38;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.25;d.maximum=2.0;d.step=0.01;d.default_value=1.0;text(d.identifier,"bassSlope");text(d.label,"Bass shelf slope");s.loudnessSettings.values[38]=1.0;}
{auto& d=s.loudnessParameters[39];d.struct_size=sizeof(d);d.index=39;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=2000;d.maximum=16000;d.step=10.0;d.default_value=6500;text(d.identifier,"trebleFreq");text(d.label,"Treble shelf frequency");s.loudnessSettings.values[39]=6500;}
{auto& d=s.loudnessParameters[40];d.struct_size=sizeof(d);d.index=40;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.25;d.maximum=2.0;d.step=0.01;d.default_value=1.0;text(d.identifier,"trebleSlope");text(d.label,"Treble shelf slope");s.loudnessSettings.values[40]=1.0;}
{auto& d=s.loudnessParameters[41];d.struct_size=sizeof(d);d.index=41;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=200;d.maximum=8000;d.step=1.0;d.default_value=2500;text(d.identifier,"presenceFreq");text(d.label,"Presence frequency");s.loudnessSettings.values[41]=2500;}
{auto& d=s.loudnessParameters[42];d.struct_size=sizeof(d);d.index=42;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=0.2;d.maximum=6.0;d.step=0.01;d.default_value=0.8;text(d.identifier,"presenceQ");text(d.label,"Presence Q");s.loudnessSettings.values[42]=0.8;}
{auto& d=s.loudnessParameters[43];d.struct_size=sizeof(d);d.index=43;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-9;d.maximum=9;d.step=0.1;d.default_value=0;text(d.identifier,"presenceGain");text(d.label,"Presence trim");s.loudnessSettings.values[43]=0;}
{auto& d=s.loudnessParameters[44];d.struct_size=sizeof(d);d.index=44;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-9;d.maximum=9;d.step=0.1;d.default_value=0;text(d.identifier,"bassTrim");text(d.label,"Bass trim");s.loudnessSettings.values[44]=0;}
{auto& d=s.loudnessParameters[45];d.struct_size=sizeof(d);d.index=45;d.kind=ENTROPY_LOUDNESS_FLOAT;d.minimum=-9;d.maximum=9;d.step=0.1;d.default_value=0;text(d.identifier,"trebleTrim");text(d.label,"Treble trim");s.loudnessSettings.values[45]=0;}
{auto& d=s.loudnessParameters[46];d.struct_size=sizeof(d);d.index=46;d.kind=ENTROPY_LOUDNESS_BOOLEAN;d.minimum=0;d.maximum=1;d.step=1;d.default_value=0;text(d.identifier,"airSafetyBypass");text(d.label,"Bypass wet safety");s.loudnessSettings.values[46]=0;}
s.parametricEqAvailable=1;s.parametricEqSlotState=EngineOptionalActive;s.parametricEqParameterCount=121;s.parametricEqSettings=defaultSettings();s.parametricEqTelemetry.module_bypass=1;std::strcpy(s.parametricEqVersion,"R10 UI schema / inactive");for(uint32_t i=0;i<121;i++){s.parametricEqParameters[i].struct_size=sizeof(s.parametricEqParameters[i]);parameterDescriptor(nullptr,i,&s.parametricEqParameters[i]);}
}
}
