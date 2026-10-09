# Nonnegative channel delay — 2026-10-09

Candidate: 0.0.9, based on 0.0.8 source commit
`312a210299c3515dc6e879e3f6c5f3de6acda820`.

Publication update: the user accepted the delay change after testing 0.0.9,
but reports that Stage3 still blocks the Stage1 source. **Source independence
remains OPEN / FAIL (user-reported)**; the cause has not been confirmed in a
new controlled reproduction. See `docs/KNOWN-ISSUES.md`. The PASS results below
are scoped to delay/control behavior and do not clear this lifecycle defect.

## Result

PASS: all 48 legacy/MUSIC/CINEMA delay parameters report a legal 0..800 ms
range. Negative IPC commands and the offline Core setter are rejected.
Negative profile values are rejected before import, including PARAM records,
MUSIC/CINEMA scenes, MANUAL_SAVE_ALL and per-channel MANUAL_SNAPSHOTS.
Fine Adjust decreases positive delays to zero and clamps there. Coarse and
Fine slider ranges expose no negative values. The upper limit remains 800 ms.

PASS: seven focused CTest targets: EntropyStage06Integration,
EntropyStage05Boundary, EntropyStage05Control, EntropyThiranDelay,
EntropyThiranValidation, EntropyAudioCoreProbe and EntropyNativeStateUnit.
See `native-tests.txt` and `native-state-tests.txt`.

PASS: actual Qt pages, separate Core process, IPC edits/readback, profile save
and device-cache refresh. Tests used a private endpoint, an offline signal and
closed DACs. See `qt-ipc-tests.txt`.

## Positive-delay compatibility

Changing the original normalized parameter range directly caused the existing
linear-delay baseline comparison to fail. That failure is preserved in
`normalization-regression-before.txt`; acceptance thresholds were not relaxed.

The final range exposes legal milliseconds 0..800 while retaining the original
FP32 encoding origin internally. It is a control encoding, not a signal offset
or a Global Base parameter. Existing accepted positive settings retain their
original values: for example, requesting 3 ms still reads back exactly
3.00006103515625 ms. The unchanged linear-delay baseline comparison and Thiran
regression tests now PASS. No processing recurrence or coefficient changes.

The initial negative-range regression test failures are preserved in
`negative-delay-red.txt` and `qt-delay-red.txt`.

## Scope and reproducibility

Build: MSVC 19.51.36257 x64 / Intel IPP 2026.0; Qt 6.11.2 / MinGW 13.1.
Run `tools/Build-Windows.ps1` with Stage04/Stage05 and the existing native
reference paths; build Qt with `tools/Build-Stage05-UI.ps1`.

Native verification:

```powershell
ctest --test-dir build/msvc-thiran -R '^(EntropyStage05Control|EntropyStage05Boundary|EntropyThiran.*|EntropyStage06Integration|EntropyAudioCoreProbe)$' --output-on-failure
```

Run `EntropyStage05Ui.exe <private-endpoint>` against a Core started with
`--control-server --endpoint <private-endpoint> --offline-test-signal`.
Use the packaged `tools/Start-Beta.ps1 -SmokeTest` for isolated executable
startup verification.

No Stage1/Stage2 processes were modified or restarted for this change. The
running 0.0.8 session was left untouched. No physical output was opened by
these tests. Stable folders and tags were not modified; no push or merge.

Historical Stage04 FAIL REALTIME and full formal V07 acceptance BLOCKED remain
visible; this focused validation does not replace those gates or establish
live listening acceptance. Complete raw build/test output remains local in
`local/update-0.0.9/`. Executable hashes are in `executable-sha256.txt`.
