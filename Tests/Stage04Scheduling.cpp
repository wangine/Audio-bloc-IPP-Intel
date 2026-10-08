#include "Stage04Scheduling.h"
#include <fstream>

namespace entropy::stage04_test {
namespace {
uint64_t ticks(FILETIME v) noexcept {return (uint64_t(v.dwHighDateTime)<<32)|v.dwLowDateTime;}
}
SchedulingProbe::SchedulingProbe(bool useMmcss):threadId_(GetCurrentThreadId()),requestedMmcss_(useMmcss) {
    dispatchError_=EnableThreadProfiling(GetCurrentThread(),THREAD_PROFILING_FLAG_DISPATCH,0,&profiling_);
    if(useMmcss){
        mmcss_=AvSetMmThreadCharacteristicsW(L"Pro Audio",&taskIndex_);
        if(!mmcss_)mmcssError_=GetLastError();
        else {priorityOk_=AvSetMmThreadPriority(mmcss_,AVRT_PRIORITY_CRITICAL)!=FALSE;
            if(!priorityOk_)priorityError_=GetLastError();}
    }
}
SchedulingProbe::~SchedulingProbe(){
    if(mmcss_)AvRevertMmThreadCharacteristics(mmcss_);
    if(profiling_)DisableThreadProfiling(profiling_);
}
SchedulingProbe::Snapshot SchedulingProbe::capture() noexcept {
    Snapshot result;LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
    FILETIME created,exit,kernel,user;
    if(GetThreadTimes(GetCurrentThread(),&created,&exit,&kernel,&user))result.cpu100ns=ticks(kernel)+ticks(user);
    else result.error=GetLastError();
    result.data.Size=sizeof(PERFORMANCE_DATA);result.data.Version=PERFORMANCE_DATA_VERSION;
    const auto error=profiling_?ReadThreadProfilingData(profiling_,READ_THREAD_PROFILING_FLAG_DISPATCHING,&result.data):dispatchError_;
    if(error)result.error=error;
    QueryPerformanceCounter(&end);result.beginQpc=begin.QuadPart;result.endQpc=end.QuadPart;
    ++reads_;if(result.error)++readErrors_;return result;
}
bool SchedulingProbe::selfTest(){
    if(!ready())return false;
    auto before=capture();
    // Prove that actual descheduling is observable; never run inside audio scope.
    Sleep(20);
    volatile uint64_t sum=0;for(unsigned i=0;i<100000;++i)sum=sum+i;
    auto after=capture();
    positiveSwitches_=DWORD(after.data.ContextSwitchCount-before.data.ContextSwitchCount);
    positiveCycles_=after.data.CycleTime-before.data.CycleTime;
    selfTestPassed_=!before.error&&!after.error&&positiveSwitches_>0&&positiveCycles_>0;
    return selfTestPassed_;
}
void SchedulingProbe::report(const std::filesystem::path& path) const {
    std::ofstream out(path);
    out<<"{\n\"thread_id\":"<<threadId_<<",\n\"mmcss_requested\":"<<(requestedMmcss_?"true":"false")
       <<",\n\"mmcss_registered\":"<<(mmcss_?"true":"false")<<",\n\"mmcss_task_index\":"<<taskIndex_
       <<",\n\"mmcss_priority_critical\":"<<(priorityOk_?"true":"false")<<",\n\"mmcss_error\":"<<mmcssError_
       <<",\n\"priority_error\":"<<priorityError_<<",\n\"dispatch_enable_error\":"<<dispatchError_
       <<",\n\"self_test_pass\":"<<(selfTestPassed_?"true":"false")<<",\n\"positive_control_switches\":"<<positiveSwitches_
       <<",\n\"positive_control_cycles\":"<<positiveCycles_<<",\n\"snapshot_reads\":"<<reads_
       <<",\n\"snapshot_errors\":"<<readErrors_<<"\n}\n";
}
}
