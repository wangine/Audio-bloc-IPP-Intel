#pragma once
#ifdef ENTROPY_KERNEL
#include <ntddk.h>
using uint8_t=UCHAR;
using uint32_t=ULONG;
using uint64_t=ULONGLONG;
using int32_t=LONG;
#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL
#endif
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffUL
#endif
#else
#include <stdint.h>
#endif
