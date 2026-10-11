#pragma once
#include <portcls.h>
void InitializeBridgeDispatch(PDRIVER_OBJECT);
NTSTATUS RegisterBridgeInterface(PDEVICE_OBJECT physicalDevice);
void DisableBridgeInterface();
void ShutdownBridgeInterface();
