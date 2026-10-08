# Stage05: native AudioCore control plane and existing Qt UI

Date: 2026-10-09. Baseline: `1fb726bc103cbb630ea55ca27647e85d897c637d`.
Branch: `codex/stage05-control-plane-qt`.
Status: written design for review; implementation has not started.

## User requirement and success contract

Start `EntropyAudioCore.exe`, then the existing visual Stage3 interface rebuilt as
`EntropyUI.exe`. All existing Crossover, Output Matrix, Headroom, MUSIC/CINEMA,
ARM/DISARM, two-DAC/device configuration and Save/Load controls talk to AudioCore.
AudioCore owns module instances, state, validation, persistence, devices and audio
buffers. Qt owns presentation and an asynchronous IPC client only. DSP parameters
are adopted at audio block boundaries, retaining native filter transitions.

No Crossover/Matrix/Headroom/ClockSync/PCM24/WASAPI mathematics changes. No hidden
normalization, input SRC, alternative interpolation, gain compensation, mandatory
AVX2 or live FIR8193 x 16 approval. Existing DSP/egress ABI remains unchanged.
Stable Stage3 sources, binaries, assignments and profiles remain untouched.
No push/merge or physical playback is included in implementation verification.

The Stage04 realtime gate is still FAIL. Physical ARM must return BLOCKED with
that explicit reason; control-plane/UI PASS does not clear the audio acceptance
gate. The old token authorizes only its bounded first-DAC test and must not be
repurposed as general two-DAC/live-profile authorization. Formal V07 acceptance
also remains BLOCKED. Offline/fake-sink tests exercise ARM state transitions;
physical WASAPI start is excluded from verification.

## Confirmed local implementation context

- Native repository currently has `IPC/Protocol.h` declarations only, no server.
  `main.cpp` exits by default; `--probe` remains an independent non-IPC fixture.
- `Engine/AudioGraph` privately owns Crossover, Output Matrix and Headroom.
  Current setters are offline/quiescent helpers, not a live control interface.
- Native Crossover already compiles immutable coefficient/configuration slots on
  the control owner and adopts a pending slot in `beginRt()` at block start.
  Its command tags report accepted/applied/failed and existing crossfades continue.
- Matrix and Headroom already have native setters and telemetry. Headroom timing,
  policy, groups and manual gain exist; no replacement gain implementation is needed.
- Stage04 already provides preallocated per-DAC FIFO/ClockSync/PCM24 pipelines,
  Input11 mapping and SinkOwner lifecycle. The existing FirstDacGraph is a restricted
  test fixture; it cannot substitute for a general two-DAC runtime.
- Existing UI: `D:/Projects/3Stage-3/Stage3/source/Host/EntropyRenderEngine/UI`,
  24 files / 243,167 bytes. `RenderWindow` owns a reference to RenderController;
  Devices loads ModuleLibrary/ModuleInstance and persists authoritative bindings
  through QSettings. Those backend dependencies must be removed in the new copy.
- Crossover/Matrix/Headroom pages already call presentation-facing `writeEngine*`
  and `readEngineTelemetry` functions, suitable for an IPC-backed facade.
- Native AudioCore: MSVC x64 + local static IPP 2026.0. Existing Qt installation:
  Qt 6.11.2 MinGW x64, GCC 13.1.0; Qt Network DLL/import library are present.
  Build UI separately with MinGW. No SDK installation or C++ cross-compiler linkage.

## Transport decision and alternatives

Recommended: native Windows named-pipe server, with asynchronous QLocalSocket on
Qt. Commands and telemetry use the same framed protocol and one control session.
No Qt dependency enters AudioCore, and no live engine pointer enters Qt.

Alternatives considered:

| Approach | Consequence |
|---|---|
| Named pipe, explicit serialized messages | Chosen: one transport, handles disconnects and correlated replies, works across compilers |
| Pipe commands + shared-memory telemetry | Lower telemetry serialization cost, but adds a second ABI, publication protocol and lifetime handling unnecessarily |
| Reuse global legacy mappings as authoritative state | Risks collision with stable Stage3 and writable UI-owned state; unsuitable for authoritative AudioCore ownership |

Wire fields use explicit little-endian integer/IEEE numeric encoding and bounded
UTF-8 strings; do not memcpy C++ object layouts or compiler padding into the wire.
The existing 16-byte header shape can be retained: version, total bytes, command,
request ID; payload envelopes add 64-bit server session and revision fields.
Maximum command/telemetry frame: 1 MiB. Profile payloads are read by the core from
paths; large serialized projects are not pasted into command frames.

The server has an isolated AudioCore endpoint, not the legacy Stage3 mapping name.
Restrict access to the current user, reject remote clients and reject an already
owned endpoint. Allow one controlling UI session; a second writer receives a
specific ownership error. Tests use unique endpoint names. Unknown version,
malformed lengths, unknown actions, NaN/Inf, invalid indices, stale sessions and
stale configuration revisions are rejected without module mutation.

Each request gets a correlated outcome: rejected, accepted/preparing, applied,
failed or superseded, with reason and authoritative revision. Accepted is not
presented as already audible/applied. No silent timeout success or replay of stale
ARM commands after reconnect.

## Ownership, scheduling and block-boundary publication

AudioCore contains distinct owners:

1. IPC/control owner: receive/decode, validate desired state, prepare native filter
   updates, own the authoritative configuration, process profile/device requests.
2. One DSP graph owner: consume validated bounded snapshots at block boundaries,
   process Crossover -> Matrix -> Headroom and fan out selected lanes.
3. DAC A and DAC B owners: independent FIFO, clock, PCM and sink state, one thread
   per enabled sink; no shared ClockSync controller/history.
4. Non-RT observer/persistence work: serialize telemetry, compute existing UI-only
   response/spectrum views and perform bounded project/device filesystem work.

Prepare all queues and buffers before graph processing. The graph consumes a
bounded preallocated SPSC control mailbox; saturation is an explicit busy/rejected
result. UI gestures and discrete operations retain ordering. Adjacent obsolete
continuous edits may only be coalesced using the existing gesture/save barriers,
never across BEGIN/END, a different parameter or a Save/Load boundary.

Crossover coefficient/FIR preparation stays off the graph callback. Reuse original
snapshot publication, command tags and crossfade; do not call serviceUpdates or
prepareToPlay from processing. Do not replace an accepted native snapshot without
reporting its actual superseded result. Applied status comes from native adoption,
which may wait for an existing crossfade; do not fabricate a block number.

Matrix/Headroom revisions enter processing only at a block boundary. Validate on
control-side candidate state before publishing, so an invalid request cannot latch
faults in the current live module through a partially executed setter. Integration
accessors may be added to Engine/AudioGraph, with its process order and calculations
unchanged. Mutable raw DSP objects must never be exposed to IPC or Qt.

Do not add callback allocations/deallocations, locks, waits, device enumeration,
file I/O, logging or IPP initialization. Stop/reconfigure operations join owner
threads outside callbacks. Profile load and device format/channel changes require
DISARM and quiescent reconfiguration; there is no unsynchronized buffer reset.

## Control coverage and Qt behavior

Inventory the original page handlers and parameter IDs before implementation;
coverage tests must fail for any omitted visible control. Do not implement just
selected sliders or a new simplified UI.

| Existing user surface | AudioCore command/state contract |
|---|---|
| Crossover parameter catalog | Original IDs, ranges, defaults/choices; no hand-selected subset or silent truncation |
| MUSIC/CINEMA and scene controls | Original scene storage/restore and native processing transitions |
| Crossover gain, trim, delay, mute/emergency, topology, IIR/FIR/All-Pass, bass/scoop and per-output settings | Validated original values and native snapshot compile/adoption |
| Gesture begin/end, side delay, Left-to-Right copy, protection reset, monitor channel | Preserve original action semantics and ordering; bulk operations commit as a defined command |
| Output Matrix | All 16 source/destination routes, many-to-one behavior, clear-all and authoritative route/status telemetry |
| Headroom | Groups, channel membership, preventive reserve, transition, policy/mode/manual gain, auto attack/release, clear fault |
| DAC A/B | Inventory, capture/resolve binding, explicit endpoint/channel pins, rate/format, enabled slots and configuration revisions |
| ARM/DISARM | Core-owned state machine; headroom/profile/device/realtime checks, precise rejection reasons; fail closed |
| SAVE AS / LOAD | Engine-owned project barrier, complete state round trip, atomic persistence and result/path telemetry |

Import only presentation files into `UI/`; keep original layout, colors, object
names, tabs, labels and interactions. Retain original numerical display units.
Change backend wiring and status text that would otherwise falsely claim a DLL is
loaded. Do not redesign controls. Use the same functions/facade names where useful.
Replace RenderController and DLL registry references with the IPC facade.

Devices must obtain inventory and binding resolutions from AudioCore, not instantiate
DeviceManager.dll or access WASAPI itself. Remove its authoritative device QSettings
writes. Qt may keep window/tab/selection preferences in a new UI-only settings
namespace, never the stable Stage3 device profile namespace. Dialog edits are
uncommitted drafts until the core validates and confirms them.

Use QLocalSocket signals/event loop; no synchronous IPC waits on the GUI thread.
On connection loss show unavailable/stale controls and stop publishing requests.
Reconnection fetches a new session's authoritative state; no replay of unconfirmed
commands or automatic ARM. Qt close/controlling-session loss requests DISARM and
core enforces a bounded control lease, retaining safe behavior when the GUI dies.

## Telemetry contract

Publish immutable per-owner snapshots after graph blocks/sink cycles. A non-RT
collector assembles versioned telemetry with server session, desired revision,
per-module applied revisions, block count, timestamp/freshness and request outcomes.
Do not read non-atomic sink/controller fields from arbitrary threads.

Preserve existing UI data: input/output meters, selected program, Crossover parameter
values and protection, response/spectrum, Matrix routes/peaks/summing/orphans,
Headroom state/reduction/timing/groups/faults, two DAC binding/sink/clock states,
FIFO fill/capacity/XRUN, ppm, negotiated format/buffer and fault reasons. Run original
response/spectrum calculations in the core observer, not in Qt or graph processing.
Reuse the existing native compatibility/display calculations rather than replacing
filter-response mathematics. Qt receives display samples only, not an audio ring.

No sample source is invented for production: input remains ordered FP64/192
L/R/C + eight CINEMA lanes, adapted to the existing 13-input ABI using two zero
unused legacy lanes. Default core launch is listening/DISARMED; source absence
is reported as waiting. Explicit offline signal fixtures permit independent UI,
processing and meter tests without Stage2 or physical sinks. Fake DAC states are
clearly marked offline and cannot masquerade as a running physical device.

Do not report unavailable Stage2, Guardian analysis or unmeasured latency as zero
or healthy. Guardian stays exact bypass; the UI must not require an active Guardian
state for health when bypass is the actual validated configuration.

## Profile compatibility and persistence

Retain `.entropy` format `EntropyProject`, format_version 1, host_engine_rate
192000 and the three physical instances crossover.main, matrix.output,
headroom.main. Preserve original native serialized state, scene snapshots, checksum
contract and unrelated opaque template fields when importing an existing project.
Read a COPY of legacy project/device settings; never overwrite the original paths.
Use a core-owned private profile directory for this branch/runtime.

Save is an ordered barrier after earlier commands have authoritative outcomes;
export one coherent desired/applied generation defined in the result. Later edits
must not leak into that saved generation. Load validates the complete candidate
before installation, DISARMs, then commits while owners are stopped; corruption,
duplicate instances, invalid checksums/version/rate or invalid DAC/headroom state
must preserve the previous profile and return an error. Successful Load remains
DISARMED, including if a saved document claims armed state. Write atomically on
non-RT owner using temporary file + replace; no Qt persistence authority.

## Verification and final evidence

- Protocol/transport: fragmented/coalesced frames, size/version/action failures,
  malformed/nonfinite values, stale revision/session, ownership, queue pressure,
  process death, reconnect and ordered correlated outcomes.
- Native control: enumerate all parameter/page-handler actions; valid and invalid
  edits, no partial invalid mutation, MUSIC/CINEMA scenes, native filter transitions,
  reset/mute and Matrix/Headroom semantics. Observe actual boundary/revision adoption.
- Profile: original `.entropy` import/export/reload, both scenes, all routes/settings,
  both DAC bindings, Save barriers, corruption and failed-load rollback, DISARM.
- Owner threads: one graph/two fake sinks, independent clocks, no GUI audio objects,
  callback allocation/lock/wait/I/O audits under command and telemetry traffic.
- Regression: all configured Stage01-04 tests, original Stage04 numeric comparisons
  and clock gate. Preserve historical realtime FAIL and rerun deadlines under IPC
  traffic; no tolerance change or declaration of hardware authorization from UI PASS.
- Qt: build the original pages with only Qt Core/Gui/Widgets/Network and protocol
  DTOs/client; source/link/dependency audit excludes DSP/IPP/WASAPI/ClockSync, module
  DLL loaders, RenderController and StageBus/audio buffers. Exercise the actual two
  executables, all page controls, returned state/telemetry, startup/reconnect and
  UI-close DISARM. Capture screens for comparison with the unchanged baseline.
- Integrity: baseline SHA256 of protected DSP/egress and all stable Stage3 UI/runtime
  files; classify each imported UI difference as backend removal/presentation data
  wiring. Report source hashes, compiler versions, executable hashes and PASS/FAIL/
  BLOCKED per gate. Keep raw test/benchmark results local; include compact evidence.

Success requires real executable/server/client integration, not a planning document
or mock UI. Physical playback and Stage2 input connection are outside verification
until their existing acceptance/authorization conditions are satisfied.

## Primary transport references

[Qt QLocalSocket](https://doc.qt.io/qt-6/qlocalsocket.html) documents the Windows
named-pipe transport and event-loop operation.
[Windows named-pipe access](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights)
documents pipe access checks; use explicit same-user permissions, not broad defaults.
