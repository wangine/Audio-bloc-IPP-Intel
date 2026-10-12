import pathlib
p=pathlib.Path(__file__).parent
s=(p/'decoder-observation/EntropySurroundDecoder-observe.cpp').read_text()
def rep(a,b):
 global s
 assert s.count(a)==1,(a,s.count(a))
 s=s.replace(a,b)
rep('struct Observation {', '''struct TraceRow {uint64_t kind,qpc,a,b,c,d,e,f;};
static std::array<TraceRow,262144> traceRows;
static std::atomic<uint64_t> traceWrite{0};
void trace(uint64_t k,uint64_t q,uint64_t a=0,uint64_t b=0,uint64_t c=0,uint64_t d=0,uint64_t e=0,uint64_t f=0){auto i=traceWrite.fetch_add(1,std::memory_order_relaxed);if(i<traceRows.size())traceRows[i]={k,q,a,b,c,d,e,f};}
struct Observation {''')
rep('DWORD written = 0;','DWORD written = 0; LARGE_INTEGER beforeWrite{};QueryPerformanceCounter(&beforeWrite);')
rep('bytes += written;','trace(5,observationTick(),beforeWrite.QuadPart,written);\n            bytes += written;')
rep('std::unique_lock lock(pcmMutex_);','auto paceEntry=observationTick();\n            std::unique_lock lock(pcmMutex_);')
rep('next += interval;', '''trace(2,observationTick(),paceEntry,queued,interval.count(),sourceGeneration);
            next += interval;''')
rep('previousRead=readTick;','trace(3,readTick,previousRead,got/24); previousRead=readTick;')
rep('DWORD flags = 0;', 'DWORD flags = 0; UINT64 devicePosition=0,deviceQpc=0;auto beforeCapture=observationTick();')
rep('capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)', 'capture->GetBuffer(&data, &frames, &flags, &devicePosition, &deviceQpc)')
rep('check(capture->ReleaseBuffer(frames), "ReleaseBuffer(UR23)");', '''trace(1,observationTick(),beforeCapture,devicePosition,deviceQpc,frames,flags,sourceGeneration);
            check(capture->ReleaseBuffer(frames), "ReleaseBuffer(UR23)");''')
rep('if(decoderStalled) ++observation.stallRestarts;', 'trace(4,observationTick(),flags,decoderStalled,decoderExited,!burstGate.locked());\n                        if(decoderStalled) ++observation.stallRestarts;')
rep('ffmpeg.stop();\n        if (workerError', '''ffmpeg.stop();
        {std::ofstream f("decoder-trace.csv");f<<"kind,qpc,a,b,c,d,e,f\\n";auto count=(std::min)(traceWrite.load(),uint64_t(traceRows.size()));for(uint64_t i=0;i<count;++i){auto&r=traceRows[i];f<<r.kind<<','<<r.qpc<<','<<r.a<<','<<r.b<<','<<r.c<<','<<r.d<<','<<r.e<<','<<r.f<<'\\n';}std::cout<<"traceRows="<<count<<" traceTotal="<<traceWrite.load()<<'\\n';}
        if (workerError''')
(p/'decoder-observation/EntropySurroundDecoder-trace.cpp').write_text(s)
cmd=(p/'decoder-observation/Build.cmd').read_text().replace('EntropySurroundDecoder-observe.cpp','EntropySurroundDecoder-trace.cpp').replace('observe.obj','trace.obj').replace('EntropySurroundDecoder.exe','EntropySurroundDecoder-trace.exe')
(p/'decoder-observation/Build-Trace.cmd').write_text(cmd)

