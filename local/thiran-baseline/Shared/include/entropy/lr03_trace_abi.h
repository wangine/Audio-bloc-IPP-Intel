#pragma once
#include <entropy/module_abi.h>
#define ENTROPY_LR03_TRACE_ID "audio.entropy.crossover.lr03-owner-trace.v1"
#define ENTROPY_LR03_TRACE_VERSION 0x00010000u
// Optional, processing-owner-only borrowed storage. Detach before destroying it.
// No audio processing is added; taps copy intermediate FP64 results.
typedef struct EntropyLr03TraceBlock {
 uint32_t struct_size, frames, channels, transition_mask, fir_mask, processed_mask;
 // role input, HP, LP/FIR, AP/blend, post-delay, post-gain
 double audio[6][16][1024];
 double delay_samples[16], gain_begin[16], gain_end[16];
 uint32_t music_role[16], cinema_role[16];
 uint32_t filter_sections[16], filter_enable[16];
 // HP/LP/AP, section, b0/b1/b2/a1/a2/z1/z2 BEFORE this block.
 double biquad[16][3][4][7];
} EntropyLr03TraceBlock;
typedef struct EntropyLr03TraceApi {
 uint32_t struct_size, extension_version;
 EntropyResult (ENTROPY_ABI_CALL *bind_owner)(EntropyModuleHandle, EntropyLr03TraceBlock*);
} EntropyLr03TraceApi;
