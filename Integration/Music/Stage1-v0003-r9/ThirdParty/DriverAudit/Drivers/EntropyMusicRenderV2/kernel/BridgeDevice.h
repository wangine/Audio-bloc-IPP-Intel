#pragma once
#include <portcls.h>
#include "../protocol/PcmBridgeV1.h"
void TraceMusicEvent(entropy::music::bridge::TraceEvent) noexcept;
NTSTATUS TraceMusicWrite(ULONG number,DWORD flags,ULONG eosBytes,NTSTATUS status,ULONG state,uint64_t generation) noexcept;
void InitializeBridgeDispatch(PDRIVER_OBJECT);
NTSTATUS RegisterBridgeInterface(PDEVICE_OBJECT physicalDevice);
void DisableBridgeInterface();
void ShutdownBridgeInterface();
