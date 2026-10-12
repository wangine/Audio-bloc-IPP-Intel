import pathlib
base=pathlib.Path('D:/Projects/3Stage-3/CinemaDecoder-v0.2.14')
out=pathlib.Path(__file__).parent/'decoder-observation'
out.mkdir(exist_ok=True)
s=(base/'source/EntropySurroundDecoder.cpp').read_text()
def replace(a,b):
 global s
 assert s.count(a)==1,(a,s.count(a))
 s=s.replace(a,b)
replace('std::atomic<bool> running {true};','''std::atomic<bool> running {true};
struct Observation {std::atomic<uint64_t> underflows{0}, gateLoss{0}, captureDiscontinuities{0}, stallRestarts{0}, exitRestarts{0}, gateRestarts{0}, underflowTick{0}, maximumReadGapUs{0};} observation;
uint64_t observationTick(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
''')
replace('if (pcmQueue_.size() < blockFrames)\n            {','if (pcmQueue_.size() < blockFrames)\n            {\n                ++observation.underflows; observation.underflowTick=observationTick();')
replace('if (!gate_.locked())\n            {','if (!gate_.locked())\n            {\n                ++observation.gateLoss;')
replace('std::array<std::uint8_t, 65536> bytes{};','std::array<std::uint8_t, 65536> bytes{};\n        uint64_t previousRead=0; LARGE_INTEGER freq{}; QueryPerformanceFrequency(&freq);')
replace('pending.insert(pending.end(), bytes.begin(), bytes.begin() + got);','''const auto readTick=observationTick();
            if(previousRead){auto gap=(readTick-previousRead)*1000000/freq.QuadPart; observation.maximumReadGapUs=std::max(observation.maximumReadGapUs.load(),gap);} previousRead=readTick;
            pending.insert(pending.end(), bytes.begin(), bytes.begin() + got);''')
replace('if (stereoTransportDiscontinuity(burstGate.locked(), flags))','if(flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++observation.captureDiscontinuities;\n            if (stereoTransportDiscontinuity(burstGate.locked(), flags))')
replace('// FFmpeg\'s SPDIF demuxer locks to the first stream.','''if(decoderStalled) ++observation.stallRestarts;
                        if(decoderExited) ++observation.exitRestarts;
                        if(!burstGate.locked()) ++observation.gateRestarts;
                        // FFmpeg's SPDIF demuxer locks to the first stream.''')
replace('<< " | decodedDiscarded=" << queue.decodedDiscarded() << "\\n";','''<< " | decodedDiscarded=" << queue.decodedDiscarded()
                      << " | pacerUnderflows=" << observation.underflows.load()
                      << " | underflowQpc=" << observation.underflowTick.load()
                      << " | pacerGateLoss=" << observation.gateLoss.load()
                      << " | captureDiscontinuities=" << observation.captureDiscontinuities.load()
                      << " | stallRestarts=" << observation.stallRestarts.load()
                      << " | exitRestarts=" << observation.exitRestarts.load()
                      << " | gateRestarts=" << observation.gateRestarts.load()
                      << " | maximumReadGapUs=" << observation.maximumReadGapUs.load()
                      << " | cinemaGeneration=" << cinema.generation() << "\\n";''')
(out/'EntropySurroundDecoder-observe.cpp').write_text(s)
cmd=f'''@echo off
call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\Common7\\Tools\\VsDevCmd.bat" -arch=x64 -host_arch=x64
cl /nologo /std:c++20 /EHsc /MD /O2 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DENTROPY_SURROUND_VERSION=\"0.2.14-observe\" /I"{base / 'source'}" "{out / 'EntropySurroundDecoder-observe.cpp'}" /Fo"{out / 'observe.obj'}" /Fe"{out / 'EntropySurroundDecoder.exe'}" /link "{base / 'build/CinemaOutput.lib'}" "{base / 'build/CinemaTransport.lib'}" "{base / 'build/EntropySurroundCore.lib'}" ole32.lib uuid.lib propsys.lib avrt.lib advapi32.lib
'''
(out/'Build.cmd').write_text(cmd)
print(out)
