#pragma once
#include <windows.h>
#include <avrt.h>
#include <stdexcept>
// Register on the capture owner before WASAPI starts. The packet processing
// path must meet the endpoint period even when other audio owners are busy.
class CaptureScheduling {
 HANDLE task_=nullptr;
public:
 CaptureScheduling(){
  DWORD index=0;task_=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);
  if(!task_)throw std::runtime_error("MMCSS capture registration failed");
  if(!AvSetMmThreadPriority(task_,AVRT_PRIORITY_CRITICAL)){
   AvRevertMmThreadCharacteristics(task_);task_=nullptr;
   throw std::runtime_error("MMCSS capture priority failed");
  }
 }
 ~CaptureScheduling(){if(task_)AvRevertMmThreadCharacteristics(task_);}
 CaptureScheduling(const CaptureScheduling&)=delete;
 CaptureScheduling& operator=(const CaptureScheduling&)=delete;
};
