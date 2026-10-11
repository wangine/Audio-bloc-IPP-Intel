#pragma once
#include <portcls.h>
#include "../protocol/PcmBridgeV1.h"
void TraceMusicEvent(entropy::music::bridge::TraceEvent) noexcept;
void InitializeBridgeDispatch(PDRIVER_OBJECT);
NTSTATUS RegisterBridgeInterface(PDEVICE_OBJECT physicalDevice);
void DisableBridgeInterface();
void ShutdownBridgeInterface();
