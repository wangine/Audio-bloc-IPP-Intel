# Authorized native live test — 2026-10-09

The user explicitly authorized physical ARM and testing, with Stage1/Stage2
untouched. The preceding Headroom correction was not sufficient for playback:
the default Core correctly refused the missing/stale Stage04 offline gate.

## Changes

- `--authorized-live-test`, exposed by `Live-Test.cmd`, authorizes only the
  current Core process. Startup remains DISARMED. Authorization is not saved
  in a profile; `offlineGatePassed` stays false. Default new Core startup keeps
  the formal gate. Input/profile/device/current-profile preflight/native faults
  and the FIR8193 x16 prohibition remain enforced.
- High-resolution owner-thread timers replace Windows sub-millisecond Sleep
  polling. Real source input is not paced a second time by the Graph scheduler.
- The native receiver uses MMCSS Pro Audio / Critical. No external source
  executable, configuration or worker is changed.
- Retry the immutable StageBus heartbeat predicate using fresh QPC time when
  a concurrent heartbeat update invalidates the earlier time snapshot. Process,
  shared fault and one-second timeout checks remain unchanged.
- IPC shutdown drains its response before disconnecting. Inbound bytes do not
  acknowledge response consumption; EOF or the fixed control lease completes
  teardown. Regression covers delayed reading and pipelined bytes.

## Retained failures and verification

Initial actual ARM opened DAC A successfully and delivered seven nonzero PCM
blocks, then failed with starvation/XRUN after 8192 frames. Receiver rate was
approximately 34 blocks/s instead of 187.5 blocks/s. Timer correction restored
187.500 blocks/s, and the transport fixture delivered 565 blocks in three seconds.

The timer-only live version still had intermittent starvation: one run delivered
23,276,544 frames before fail-closed shutdown. Average rates in its 68.38-second
capture were identical (approximately 192,008.8 frames/s), but delivery contained
a burst deficit of eight blocks. That version did not yet give the Core ingress
worker MMCSS priority. These failures are retained, not rewritten as PASS.

Native CTest: 33/33 PASS in 121.29 seconds after the scheduling/MMCSS changes.
Following the final IPC drain correction, all five affected IPC/startup/transport
tests PASS; authorization test uses 30 fresh Core processes, no actual source/DAC.
Independent review: no remaining Critical/Important findings.

Current user profile, dynamic DAC assignment and original DSP settings are
restored from a private snapshot; MANUAL Headroom stays -4 dB. DAC A is configured
for eight channels, 192 kHz, PCM24-in-32; DAC B stays disabled/unbound. Guardian
is exact bypass. No endpoint is hardwired into product startup or the test switch.

Source integrity: 101 protected DSP/Egress/Devices/Shared/Transport files and
28 stable Stage3 files match their recorded baselines. Qt imports remain IPC-only.
No push or merge. MSVC 19.51.36257, Intel IPP 2026 static.

Core SHA256:
`E6D7CBABE8D6EE30481FA559BD6E69B38AB072BBBBACAF7F7B3CDFE49C42B8DD`.
Qt SHA256:
`2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`.

The latest formal Stage04 benchmark remains FAIL: 79/80 rows, three strict
deadline misses. V07 formal acceptance remains BLOCKED. Manual hardware testing
does not assert production realtime qualification or auditory quality.

Raw private logs, profiles and full telemetry remain in ignored `local/stage07/`,
including every `authorized-live-*`, `high-resolution-*`, `ingress-mmcss-*` and
`final-*` result. Final hardware observation is recorded below after execution.

## Final hardware observation

PASS for the observed manual session (over five minutes): DAC A RUNNING /
ClockSync LOCKED, eight channels at 192 kHz, 67166208 accepted
frames and 64764 nonzero PCM blocks. Zero XRUNs, native/sink
faults and recorded sink deadline misses. Maximum owner wall time excluding
event wait: 4136.4 us; this can still include
scheduling delay and is not pure DSP CPU cost. Closing Qt did not interrupt
audio: another 1958912 frames were accepted during the ten-second UI-closed
measurement (telemetry polled each second to retain the five-second IPC lease).
Headroom stayed -4 dB. Core and Qt were left running for user listening.
This finite observation does not guarantee indefinite realtime stability.

Evidence: `local/stage07/final-live-ui-closed.json`, final ARM/preflight and
launch logs; public compact result `authorized-live-result.json`. Earlier
timer-only failures and formal Stage04 FAIL remain retained.
