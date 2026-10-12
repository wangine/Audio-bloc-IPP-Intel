import pathlib

p = pathlib.Path(__file__).parent
out = p / 'decoder-observation-v19'
s = (out / 'EntropySurroundDecoder-trace.cpp').read_text()

def replace(old, new):
    global s
    assert s.count(old) == 1, (old, s.count(old))
    s = s.replace(old, new)

replace('struct Observation {', '''static std::array<unsigned char, 128 * 1024 * 1024> rawCarrier;
static uint64_t rawBytes = 0;
static bool rawOverflow = false;
struct Observation {''')
replace('if (bufferResult == AUDCLNT_S_BUFFER_EMPTY ||', '''trace(7,observationTick(),beforeCapture,static_cast<uint32_t>(bufferResult),
                  frames,flags,devicePosition,deviceQpc);
            if (bufferResult == AUDCLNT_S_BUFFER_EMPTY ||''')
replace('if (frames == 0) break;', '''if (frames == 0) break;
            const auto rawSize = uint64_t(frames) * 4;
            const auto rawOffset = rawBytes;
            if (rawSize <= rawCarrier.size() - rawBytes) {
                if (data != nullptr && !(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                    std::memcpy(rawCarrier.data()+rawBytes, data, static_cast<size_t>(rawSize));
                else
                    std::memset(rawCarrier.data()+rawBytes, 0, static_cast<size_t>(rawSize));
                rawBytes += rawSize;
            } else { rawOverflow = true; }
            trace(6,observationTick(),rawOffset,rawSize,flags,devicePosition,deviceQpc,
                  reinterpret_cast<uintptr_t>(data));''')
replace('check(capture->ReleaseBuffer(frames), "ReleaseBuffer(UR23)");', '''const auto released = capture->ReleaseBuffer(frames);
            trace(8,observationTick(),static_cast<uint32_t>(released),frames);
            check(released, "ReleaseBuffer(UR23)");''')
replace('std::thread capture([&] {', '''// Allocate and touch diagnostic pages before the capture owner starts.
        std::fill(rawCarrier.begin(), rawCarrier.end(), 0);
        std::thread capture([&] {''')
replace('ffmpeg.stop();\n        {std::ofstream', '''ffmpeg.stop();
        { std::ofstream raw("carrier-48000-s16le-stereo.raw", std::ios::binary);
          raw.write(reinterpret_cast<const char*>(rawCarrier.data()),rawBytes);
          std::cout << "rawBytes=" << rawBytes << " rawOverflow=" << rawOverflow << '\\n'; }
        {std::ofstream''')
(out / 'EntropySurroundDecoder-raw-trace.cpp').write_text(s)
cmd = (out / 'Build-Trace.cmd').read_text().replace(
    'EntropySurroundDecoder-trace.cpp','EntropySurroundDecoder-raw-trace.cpp').replace(
    'trace.obj','raw-trace.obj').replace(
    'EntropySurroundDecoder-trace.exe','EntropySurroundDecoder-raw-trace.exe')
(out / 'Build-Raw-Trace.cmd').write_text(cmd)
