# 0.0.8 — measured V07 host integration correction

Date: 2026-10-09. Baseline: 0.0.7 / 6ac72f2be86fca4b14801c94265eabb2771600e5.
Stable listening reference 0.0.3 is preserved. No push or merge.

## Status

- PASS: MSVC x64 / IPP and Qt builds; nine focused native checks; actual Qt-to-Core offline IPC controls/profile refresh.
- PASS: scoped native versus actual V07 Crossover oracle, 90 cases / 1440 channel rows, including eight current-profile cases. Not bit-exact.
- PASS: three clean Core stop/start cycles against the unchanged real publisher; three single-command dual-DAC ARM checks (five seconds each); no XRUN, nonfinite or sink fault in those checks.
- FLAGGED: finite PCM24 overrange/clipping on DAC A in the final live programme. Audio listening acceptance is PENDING USER. Do not call this audio-quality PASS.
- Historical Stage04 remains FAIL REALTIME; full formal V07 acceptance remains BLOCKED. These checks do not replace those acceptance suites.

## Corrections and reproduced failures

1. Original Headroom UI was missing: copying exact V07 source retained an older timestamp than the existing object, and incremental Ninja skipped rebuilding it. Recompiled both Qt executables; real V07 controls and complete native telemetry now pass through IPC. Page SHA256 remains DAD322E4107BB6EB0B5FBFBC131B788BAEC513421315E3257D0A52B601456698.
2. Restore V07 global16 host ledger, declared MUSIC reserve and Matrix contributor reserve. Preserve actual user AUTO/manual capacity, ATTACK/RELEASE, transition and mode. CINEMA input meters use actual Matrix output. Repeated identical state does not restart AUTO ramps.
3. Source-mask regression reproduced: inactive CINEMA route was counted as a second contributor, imposing an extra theoretical 6.0206 dB. Matrix now uses the captured block-owner topology and retains all contributors during native preparation/crossfade. Dormant routes survive. A separate test proves unpublished control APVTS changes cannot truncate the frozen block. No Matrix summing mathematics changed.
4. Restore V07 bounded ingress recovery, rebase graph-only restart without releasing the peer, sample publisher heartbeat before the local timestamp. Receiver-local validation does not poison shared fault state. Closing Core drains the request and releases its lease; Qt closure retains Core. There is no direct Stage1 connection and no DSP module transport connection.
5. Cache only a successful strict preflight for identical parameters, saved scenes, Matrix and Headroom settings. Exact comparison prevents changed settings from reusing it; a failed validation clears the old result. Monitor and gesture commands were reproduced unnecessarily invalidating the cache; fixed without weakening deadline, nonzero-output or fault gates.
6. Dual-clock recovery stops accelerated production when any armed sink recovers its cushion, matching V07. ClockSync interpolation/controller, PCM24 and WASAPI mathematics unchanged.

The old unbounded sender produced an observed 547.7 us recovery gap. An intermediate test incorrectly gated upstream dequeue observations: it failed at 3993.6 us because producer-thread scheduling affects those observations. The failure is retained. Final test prints both diagnostics: producer observation 3796.4 us, actual receiver send minimum 4669.1 us. The same 4000 us transport gate now evaluates actual send timing. DSP deadline gates are unchanged.

## Numerical results

Actual V07 DLL SHA256: 1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A.
Maximum absolute error: 7.8304029926812291e-13; maximum channel RMS error: 2.0606217911781914e-13.
Nonfinite: 0. Bit mismatches: 25144952 (reported, not concealed).
Original limits: max <= 1e-12 + 1e-11 * reference peak; RMS <= 1e-13 + 1e-12 * reference RMS.
Positive control injects 0.001 error and exits FAIL; no tolerance relaxation.

All 1440 first-response indices match. All 352 impulse rows match both first response and strongest peak indices (340 nonzero, 12 expected inactive). Ninety periodic FIR-tone rows select another repeated absolute maximum: 78.125 Hz shifts are whole multiples of 6144 samples; 1500 Hz shifts are 128 samples. Their max error is <=9.71445e-17, peak amplitude difference <=1.38778e-17. These are repeated-peak ambiguities, not impulse latency differences; not a claim of exact sub-sample phase identity.

FIR direct impulse peak: 2049 taps = 1024 samples / 5.333333 ms; 8193 taps = 4096 samples / 21.333333 ms, both versions. PCA + FIR: 1150 / 4226 samples, both. Current-profile CH8 peaks: MUSIC 3009, CINEMA 2456 samples, both. IIR/PCA impulse maxima are response-shape diagnostics, not frequency-independent group delay.

Scope: MUSIC/CINEMA, extractor1 and PCA3, bypass/LR24/LR48/FIR2049/FIR8193 offline, impulse/tone/deterministic noise and program transitions; 11830 parameter readback rows PASS. Baseline comparison uses legacy linear delay, all retired Global Base values explicitly zero; it does not certify Thiran against V07. CSVs retain per-channel amplitude/error/timing diagnostics. No FIR8193 x16 live test.

## Final live measurements

Profile controls and DAC choices came from the user's saved current session, never hardcoded into product sources. Three ARM commands each succeeded on the first attempt; unchanged later calls reused validation. Both DACs ran at 192 kHz / eight channels / PCM24. Guardian exact bypass. Five seconds per run, 1024 frames; bounded observations, not long-term certification.

| ARM | Command ms | Cache | DAC A overrange samples | DAC A clipped blocks | DAC B overrange samples |
|---|---:|---|---:|---:|---:|
| 1 | 1892.74 | False | 23180 | 58 | 0 |
| 2 | 92.32 | True | 50375 | 116 | 0 |
| 3 | 102.14 | True | 20586 | 52 | 0 |

Profile preflight median/p99/max: 840.9 / 1098.5 / 1255.5 us; 1000 measured blocks after 940 warmup; deadline 5333.333 us; zero misses. Live graph callback wall time and scheduler lateness are separate fields in summary.json; wall time is not exclusive CPU execution time. No new ETW scheduling attribution claimed.

Live clipping is real evidence relevant to reported distortion, not evidence of an 8-bit signal path. AUTO has only 3 dB selected capacity in the user profile; the original controller has attack/release and finite capacity. No gain/capacity was changed, no limiter added and no clipping counters hidden. A listening test must assess the programme and chosen policy; this report does not claim to have heard physical analogue output.

Stage1 PID 22200 and publisher PID 12656 remained running; the three native clean shutdown/restarts resumed fresh audio without upstream restarts. V07 was also launched from a private runtime copy and closed normally without changing those publisher processes. No Stage1/Stage2 or immutable V07 files modified. Arbitrary forced consumer death/passive-protocol independence is not certified by clean lifecycle checks.

## Integrity and reproduction

No diff against 0.0.7 under DSP/, Egress/, Devices/, Shared/. Headroom engine remains original V07. All changes are host integration, IPC projection, existing UI page restoration, transport cadence or test/build support. Per-source and executable SHA256 in source-and-build-sha256.json. Compiler MSVC 19.51.36257 x64 / Intel IPP 2026.0; Qt 6.11.2 / MinGW13.1.

Build native with tools/Build-Windows.ps1 -BuildFolder build/msvc-thiran -Stage04 -Stage05 -CompileOnly and the documented existing native-reference paths. Build Qt with tools/Build-Stage05-UI.ps1 -BuildFolder build/qt-thiran. Focused CTest names are in native-tests.txt. Optional oracle: configure ENTROPY_V07_INTEGRATION=ON and ENTROPY_V07_DLL to the exact hash-checked DLL; run EntropyV07Integration <DLL> <results-folder> [private-profile-path]. An incorrect/missing reference fails explicitly. Keep private profiles and raw live snapshots local.

Raw evidence, full live snapshots, copied user session and expected failures remain under ignored local/update-0.0.8. No executables, SDK libraries, build directories or private profiles are tracked.
