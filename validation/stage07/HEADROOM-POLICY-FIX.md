# User-owned Headroom policy — 2026-10-09

The integration previously required MANUAL SAFE attenuation of at least 6 dB and coverage of the diagnostic ledger. It also rejected BIT PERFECT and native-valid inactive masks. These host restrictions were removed at the user's explicit request. They were not DSP requirements of the original Headroom engine.

Headroom retains its original AUTO, MANUAL SAFE and BIT PERFECT implementations. The host does not change the selected mode, manual gain, active mask, transition, coefficients or output samples to satisfy ARM. Original configuration faults, nonfinite state/output and the separate transport/device/deadline gates remain effective. Ledger values remain diagnostic telemetry.

The independent profile timing measurement now uses MMCSS Pro Audio / Critical, matching the graph scheduling class, with exception-safe restoration. Its original 1024/192000 deadline (5333.333 us) and zero-miss acceptance remain unchanged. Wall-clock callback cost may still include scheduling. This change does not modify the live DSP or sink processing code.

A cloned profile warms for at least five virtual seconds and until its original mode ramp settles, within the existing 20-second wall budget. It does not shorten a user transition. This supports the native maximum 10000 ms transition, which the previous fixed five-second clone warmup incorrectly rejected.

## Verification evidence

- Regression RED: previous host rejected MANUAL -4 dB with its minimum/ledger error.
- Regression RED: maximum native 10000 ms transition failed at fixed five-second clone warmup.
- Targeted GREEN: Stage07 live workers, profile preflight, original Headroom unit and allocation tests: 4/4 PASS.
- User manual gains -4, -1, 0 and -12 dB accepted without mutation. All 16 channels match the original scalar exactly.
- Original BIT PERFECT preserves signed `-0.0` bit-exactly on all tested samples.
- AUTO ramps and original fault handling remain tested.
- IPC preflight covers MANUAL -4 / 0, BIT PERFECT, AUTO, and MANUAL -4 with a 10000 ms transition; verifies unchanged mode, gain and timing.
- Read-only review: no remaining Critical or Important findings after the maximum-transition correction.

Detailed RED/GREEN logs and each complete-suite execution remain under ignored `local/stage07/`. Earlier failures are retained, including one unprioritized IPC preflight deadline miss and test-fixture failures caused by checking the initial unprocessed mode or assuming BIT PERFECT → unity AUTO must ramp. Neither DSP tolerances nor deadline thresholds were relaxed.

The running user configuration was saved before replacement, including MANUAL -4 dB and the user's DAC A assignment. It is a private session snapshot, not a hardwired product configuration. No mother/example profile was overwritten.

## Final executable and current user profile

Core SHA256: `E1412BBF980E2A9C7171ABA046D3A87C582465E17316C2E9FF2A57584E84A0E3`

UI SHA256: `2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`

MSVC x64 19.51.36257; Intel IPP 2026.0 static.

The corrected Core was started with the saved current configuration. Native IPC confirmed identical MANUAL -4 dB and DAC configuration. The actual user-profile independent offline preflight passed 1000 callbacks at 1024/192000: median 751.15 us, p99 1036.9 us, maximum 1391.4 us, zero misses against 5333.333 us, nonzero output peak 0.000784958277429854. MMCSS registration/critical priority both succeeded. No physical device was opened.

Final complete release execution:

| Gate | Result |
|---|---|
| Native CTest | PASS 32/32 |
| Qt controls/profile/startup/selection/scroll/IPC checks | PASS |
| Numeric comparisons | PASS 96/96; zero absolute/RMS/phase/latency/PCM differences |
| Clock drift | PASS 8/8 |
| Realtime processing allocation/lock/wait/I/O audit | PASS: recorded counters zero |
| Benchmark strict wall deadlines | FAIL: 79/80 rows, 3 misses |
| Protected source integrity | PASS: 101 module/transport files, 28 stable Stage3 files |
| Actual current-profile Headroom/preflight | PASS; user settings unchanged |
| Physical ARM eligibility / hardware execution | BLOCKED / NOT EXECUTED |

The three misses occurred in ClockSync, MUSIC, PCM24-in-32, 256 frames: wall durations 2049.0, 2035.6 and 2115.8 us against the unchanged 1333.333 us deadline. Each recorded one context switch within its measurement bracket, with no profiling error. This is evidence of descheduling in the bracket, not exact CSwitch/DPC/ISR attribution inside the callback. Thread-cycle measurements were respectively 96600, 75600 and 149700. No DSP change or acceptance relaxation was made to hide these failures. Physical ARM remains blocked by this separate clock/deadline gate, rather than Headroom gain or ledger policy.

Detailed final evidence: `local/stage07/headroom-policy-release/`, actual user-profile verification: `local/stage07/user-headroom-final-verification.json`. Previous complete PASS runs and every historical FAIL remain retained. No push or merge performed.
