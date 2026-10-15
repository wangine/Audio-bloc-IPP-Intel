# MUSIC ClockFollow C1 — driver-side capability, version 0.3.1.0

15 October 2026. Repository: `wangine/Audio-bloc-IPP-Intel`.
Branch: `chatgpt/music-clockfollow-c1-20261015`.
Source base: `fa9e126d4bcbde2aaaf033de78509c6f601f8dbb`, pinned by the inspected installed 0.3.0.11 package. The user's report found all 177 driver-tree files matching that package's source manifest. This is not the old live-renderer branch and must not be merged wholesale into it.

## What was implemented

C1 adds an optional fine-rate actuator to the MUSIC virtual WaveRT driver, not a resampler and not a physical DAC sink. Its default remains autonomous QPC pacing. The original nominal arithmetic and its fractional pause accounting remain in use until an explicit controller lease is acquired. The existing PCM provider and Read/Info/Claim/Trace layouts remain unchanged.

`model/Clock.*` supports signed parts-per-billion rate correction with rational fractional phase retained across updates. An update integrates the old rate to its application timestamp and changes only future rate: it never retroactively multiplies the elapsed past by the new rate, jumps the position, edits PCM, or retires a queue. No floating-point arithmetic, allocation or unbounded loops are used in this clock model.

`model/ClockFollow.h` owns the bounded lease/state rules. `protocol/ClockBridgeV1.h` exposes five additive METHOD_BUFFERED operations: Capabilities, Status, Acquire, Update, Release. The wire structure sizes, versions, kind/code agreement and reserved fields are validated. Replies carry explicit QPC frequency/ticks, stream generation, master epoch, sequence, requested/applied rate and expiry. This is a RATE actuator; it does not accept arbitrary cursor writes or hardware pointers.

`kernel/BridgeDevice.cpp` and `ClockIoctl.h` connect those operations to the same live stream's `PcmCommit` and `WaveRtTimeline`. Clock ownership is tied to a file-handle lease, separate from the sole RAW PCM reader claim. Old source generations, old master epochs, duplicate/out-of-order sequences, foreign lease updates and out-of-range rates are refused. PAUSE/STOP invalidate the clock lease. A new active source session requires reacquisition.

The rate bound is +/-1,000,000 ppb (+/-1000 ppm). Each accepted update limits applied change to 20 ppm per elapsed QPC second; this is a bound on update steps, not a continuously interpolated analog frequency ramp. The renewable lease is two seconds. Between updates the last correction holds. At expiry the rate becomes nominal at the exact model deadline, with continuous position and explicit EXPIRED status. Release also returns to nominal without moving phase. These are explicit fallback slope changes, not a claim of uninterrupted audio across loss of the master. A later direct renderer must treat missing/expired synchronization as unavailable and stop/wait rather than conceal drift.

Every position/count/notification decision continues to use one WaveRT timeline. `PcmCommit::Submit` now samples its clock-evaluation QPC together with the cached clock under the existing metadata lock. This prevents evaluating an older DDI-entry timestamp against a just-rebased clock. The original DDI-entry timestamp remains in the packet header. Payload copying and packet admission formulas are not replaced. Therefore C1 does include this small kernel scheduling/coherence change even while rate control is inactive; it is not a claim that every instruction of .11 is unchanged.

## Ownership, real-time and security limits

Control handling is PASSIVE_LEVEL, validates file read/write access and tries the existing publication operation gate once. It does not block inside a timer or copy PCM under a spin lock. The bounded rate mutation and cached clock/QPC boundary share the existing metadata lock; no new DAC IO, ring resize or logging is added to the timer. Cleanup releases the handle's lease when the operation gate is available; otherwise the bounded expiry remains effective. Kernel stream ownership retains the existing rundown protection.

Lease tokens establish identity, not authentication of a particular executable. The inherited media-device ACL permits ordinary local media applications to open the device. C1 does not claim a hardened service-only clock-control endpoint or change the existing ACL. Before deployment of the complete bridge, explicitly review which service/user is authorized to acquire clock control and whether a separately restricted control interface is required. Do not deploy this unsigned candidate as a production security qualification.

The core/model and WaveRT binding tests run in user mode. They do not qualify kernel IRP cancellation, hostile-handle races, actual DPC scheduling, arbitrary source stalls or Windows-driver operation after installation. Kernel build, signing and loaded-driver validation are separate steps.

## What C1 deliberately does not change

No Stage1/Stage2/Stage3 code, AUX driver, CINEMA decoder, WDL, r8brain, matrix, crossover, Headroom or loudspeaker profile is changed. No bridge to an actual selected DAC is present yet. No per-DAC clock observation or direct-primary mode is enabled. Compiling C1 alone cannot improve or change the current listening session.

The remaining integrated work is: a single authorized ClockBridge controller, coherent master observations from Stage3, Stage1 scheduling that does not impose an independent rigid nominal QPC clock, and an explicitly admitted DIRECT_PRIMARY mode. Simultaneous AUX must first be synchronized before its mix is declared master-domain PCM. No scheme can preserve uninterrupted finite-buffer operation when required upstream data never arrives.

The user identifies CINEMA input as external optical reception through UR23. Encoded DD/DTS in an apparent 16/48 stereo carrier is not ordinary decoded stereo PCM. Do not adjust gain, resample or alter those transport words before depacketization/decoding. Unlike the software MUSIC endpoint, the optical source is not controllable by this driver's rate actuator. A future conversion/adaptation acts on decoded PCM; reuse the decoder's existing SRC for variable-ratio conversion only if its implementation supports that correctly. C1 leaves the current CINEMA path untouched. A 16/48 carrier label alone does not establish the actual decoded channel count.

## Evidence available before the local build

The deliberately failing commit `1cbf734c5639ed45fa85cf48907d99fb04445101` compiled and failed at runtime with: `Clock has no external fine-rate actuator; only nominal QPC pacing exists`.

The subsequent CI builds the actual clock, lease, ABI and OS-independent WaveRtTimeline sources on Windows/MSVC and Linux. It verifies continuity across signed updates, deadline expiry, pause/reset, lease/generation/replay rejection and all 12 existing PCM format combinations. The packet tests retime 128 packets per format and compare every delivered payload byte, including container bytes. Windows also runs the unchanged Model/Bridge/KernelBinding/Inbox test sources, including the independent 128-bit integer oracle. These are offline tests with no audio devices or installed driver.

PowerShell delivery scripts are syntax-checked in Windows CI without executing the kernel build or touching the machine. Passing CI is not a claim that `BridgeDevice.cpp`, `PcmCommit.cpp` or the full SYS have linked against the user's WDK. That kernel build is the purpose of the local handoff.

## Local compile-only delivery

Read `CLOCKFOLLOW-C1-CODEX.md`. From the isolated new worktree run:

`Drivers/EntropyMusicRenderV2/tools/Build-C1.ps1 -ExpectedCommit <exact supplied SHA>`

The script builds/runs only the bounded C1 model-contract executable, then builds the kernel candidate with the existing WDK and extracted SDK. It creates an unsigned SYS and stamped INF plus identity/version/hash records in a new candidate folder. It does not build a signed/install-ready package, create a catalog, install/import a certificate, change test-signing/Secure Boot, register an INF, restart an endpoint, read from the live RAW provider or contact a DAC.

SYS FileVersion, stamped INF and Info driverBuild all identify 0.3.1.0 / 0x00030100. The previously installed 0.3.0.11 and its signed package remain untouched. `INCOMPLETE.txt` remains on failure; successful compilation removes that marker but retains `NOT-FOR-INSTALLATION.txt`.

Stop at this candidate. Do not run the old deployment or acceptance scripts: their earlier installation approvals do not apply to C1. No new live listening test is possible until the bridge/Stage1/direct-primary integration is supplied and deployment is explicitly approved.
