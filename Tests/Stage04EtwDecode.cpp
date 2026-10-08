#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace {
constexpr GUID threadProvider{0x3d6fa8d1,0xfe05,0x11d0,{0x9d,0xda,0x00,0xc0,0x4f,0xd7,0xba,0x7c}};
constexpr GUID perfProvider{0xce1dbfb4,0x137e,0x4da6,{0x87,0xb0,0x3f,0x59,0xaa,0x10,0x2c,0xbc}};
struct Cpu {uint32_t tid=0;int64_t start=0;bool known=false;};
std::map<unsigned,Cpu> cpus;std::set<uint32_t> targets;
std::ofstream intervals,interrupts;uint64_t switches=0,irqCount=0,mismatches=0,invalidIrq=0;
int64_t first=INT64_MAX,last=0;
void WINAPI event(EVENT_RECORD* record){
    const auto time=record->EventHeader.TimeStamp.QuadPart;
    const auto opcode=record->EventHeader.EventDescriptor.Opcode;
    const unsigned cpu=record->BufferContext.ProcessorIndex;
    if(IsEqualGUID(record->EventHeader.ProviderId,threadProvider)&&opcode==36&&record->UserDataLength>=8){
        uint32_t next,old;std::memcpy(&next,record->UserData,4);std::memcpy(&old,static_cast<char*>(record->UserData)+4,4);
        auto& state=cpus[cpu];
        if(state.known){if(state.tid!=old || time<state.start)++mismatches;
            else if(targets.contains(old))intervals<<cpu<<','<<old<<','<<state.start<<','<<time<<'\n';}
        state={next,time,true};++switches;first=(std::min)(first,time);last=(std::max)(last,time);
    } else if(IsEqualGUID(record->EventHeader.ProviderId,perfProvider)&&(opcode==66||opcode==67||opcode==68||opcode==69)&&record->UserDataLength>=8){
        int64_t start;std::memcpy(&start,record->UserData,8);
        if(start>time||start<0)++invalidIrq;
        else {interrupts<<cpu<<','<<start<<','<<time<<','<<unsigned(opcode)<<'\n';++irqCount;}
        last=(std::max)(last,time);
    }
}
}
int wmain(int argc,wchar_t** argv){
    if(argc<4){std::cerr<<"usage: EntropyStage04EtwDecode trace.etl output-folder target-tid [target-tid...]\n";return 2;}
    try {
        std::filesystem::path output=argv[2];std::filesystem::create_directories(output);
        for(int i=3;i<argc;++i)targets.insert(static_cast<uint32_t>(std::stoul(argv[i])));
        intervals.open(output/"running-intervals.csv");interrupts.open(output/"interrupt-intervals.csv");
        intervals<<"cpu,thread_id,start_qpc,end_qpc\n";interrupts<<"cpu,start_qpc,end_qpc,opcode\n";
        EVENT_TRACE_LOGFILEW file{};file.LogFileName=argv[1];
        file.ProcessTraceMode=PROCESS_TRACE_MODE_EVENT_RECORD|PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        file.EventRecordCallback=event;auto handle=OpenTraceW(&file);
        if(handle==INVALID_PROCESSTRACE_HANDLE){std::cerr<<"OpenTrace error "<<GetLastError()<<'\n';return 2;}
        const auto result=ProcessTrace(&handle,1,nullptr,nullptr);CloseTrace(handle);
        // Finish only known target intervals; benchmark callbacks end before logger stop.
        for(auto [cpu,state]:cpus)if(state.known&&targets.contains(state.tid)&&last>state.start)
            intervals<<cpu<<','<<state.tid<<','<<state.start<<','<<last<<'\n';
        std::ofstream metadata(output/"trace-metadata.json");
        metadata<<"{\n\"process_trace_error\":"<<result<<",\n\"events_lost\":"<<file.LogfileHeader.EventsLost
            <<",\n\"buffers_lost\":"<<file.LogfileHeader.BuffersLost<<",\n\"qpc_frequency\":"<<file.LogfileHeader.PerfFreq.QuadPart
            <<",\n\"clock_type\":"<<file.LogfileHeader.ReservedFlags<<",\n\"cswitch_events\":"<<switches
            <<",\n\"interrupt_events\":"<<irqCount<<",\n\"state_mismatches\":"<<mismatches<<",\n\"invalid_interrupt_intervals\":"<<invalidIrq
            <<",\n\"first_qpc\":"<<first<<",\n\"last_qpc\":"<<last<<"\n}\n";
        const bool pass=result==ERROR_SUCCESS&&!file.LogfileHeader.EventsLost&&!file.LogfileHeader.BuffersLost
            &&switches>0&&!mismatches&&!invalidIrq;
        std::cout<<"ETW decode: switches="<<switches<<" interrupts="<<irqCount<<" state_mismatches="<<mismatches<<" status="<<(pass?"PASS":"BLOCKED")<<'\n';
        return pass?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
