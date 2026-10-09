# Beta test 0.0.1 publication verification

Date: 2026-10-09. Branch: `codex/stage07-live-egress`. No merge into main.

## Results

- Native CTest: **33/33 PASS**, 132.11 seconds. `EntropyAudioCore.exe --probe`: PASS.
- Qt original control pages, executable IPC smoke, profile first-run flow,
  device selection/refresh/scroll and bounded IPC preflight tests: PASS.
- Portable packaged Core + Qt smoke with the installed Qt/MinGW/IPP SDK paths
  removed from PATH: PASS. The test uses a unique offline endpoint, never the
  actual source or DAC. Windows PowerShell 5 runs the bundled launcher.
- Integrity: **101 protected native source files and 28 stable Stage3 files
  match baseline**; Qt binary imports remain IPC-only.
- Source review: no unresolved Critical/Important findings. The generated
  isolated ingress fixture registers its source in CMAKE_CONFIGURE_DEPENDS so
  future production ingress edits regenerate the fixture.
- MSVC x64 19.51.36257, static Intel IPP 2026.0; Qt 6.11.2/MinGW 13.1.

**PASS NUMERIC / FAIL REALTIME; formal V07 acceptance BLOCKED.** These functional
tests do not replace the strict performance gate. Existing numeric results are
96/96 with zero absolute/RMS/phase/latency/PCM differences, clock 8/8 PASS.
The existing strict benchmark retains 79/80 passing rows and three deadline
misses. See ../stage07 for measurements, historical failures and the finite
manual DAC A observation. FIR8193 x16 remains forbidden live; DAC B has not been
physically tested. No new physical output test was started during publication.

## Failures retained and resolved during packaging

The first full CTest run was **32/33 PASS**: the Stage06 ownership contention
fixture could not acquire the named render-owner mutex held by the user's
running live Core. The test now compiles the identical ingress implementation
with an isolated fixture mutex name; production ownership and ABIs are unchanged.
The contention and recovery assertions remain active, with no relaxed tolerance
or skipped test. Full rerun: 33/33 PASS. After adding the configure dependency,
the affected target was rebuilt and its test passed again (5.55 seconds).

Portable launcher attempts exposed an empty startup-log null dereference and a
missing Qt offscreen platform plugin. The launcher now waits for non-null
readiness output, and the package includes qoffscreen.dll alongside qwindows.dll.
Final smoke PASS; private attempt logs remain under local/releases/0.0.1.

## Release contents and checksums

All source engines, integration, CMake, tests, scripts and public documentation
are committed. The runtime ZIP includes Core, the IPC-only Qt interface, its
runtime dependencies, launchers, public reports and third-party notices. No SDK,
static library archive, actual build directory or private profile is uploaded.
Complete raw Stage07 evidence is retained locally; it is not a release asset.
The package manifest identifies the clean source commit and hashes every
included payload file. Release assets have a separate SHA256SUMS.txt.

Core SHA256:
`E6D7CBABE8D6EE30481FA559BD6E69B38AB072BBBBACAF7F7B3CDFE49C42B8DD`

Qt SHA256:
`2ECF91A945AD314DE99FC7651143C89109622D370DAE691203F050F4F93F3CC3`

The production Core and Qt were neither rebuilt nor restarted for packaging;
the packaged binaries are the tested live binaries. Closing Qt still leaves
the separate Core process running. New sessions always start DISARMED; physical
test authorization is explicit and process-scoped, not stored in a profile.
