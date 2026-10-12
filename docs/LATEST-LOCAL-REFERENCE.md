# Latest local reference — 13 October 2026

MUSIC and CINEMA use independent passive contracts. Launch order is arbitrary;
an absent source causes mute while Stage 3 retains the user's ARM intent.
The source and DSP stages do not start or control one another.

| Component | Local runtime / launcher | Reference |
|---|---|---|
| Stage 1 | `D:\Projects\3Stage-3\Stage1-v0003-r9\release\stage1-native-music-v0003-r9\Start-Stage1-Music.cmd` | Native MUSIC/AUX source r9, unchanged during this Cinema repair |
| Stage 2 | `D:\Projects\3Stage-3\Stage2\release\stage2-music-v0010\ColdStart-DSP-Music.cmd` | MUSIC 2ch IN / LRC 3ch OUT; LOAD/SAVE and last-profile restoration |
| Stage 3 | `D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.9-v2-r3\EntropyAudioCore-0.1.9-v2-r3-64k-win-x64\Live-Test.cmd` | Actual tested 64k build, source-reference startup correction r3 |
| Cinema decoder | `C:\Program Files\Entropy Audio\Surround Decoder\Entropy Surround Decoder.exe` | Installed v0.2.21; original FULL AUTO / stereo 5.1/7.1 / tray / autostart |
| Decoder sources and installer | `D:\Projects\3Stage-3\CinemaDecoder-v0.2.21` | All modified source, original GUI/installer wrapper, runtime and evidence |

Stage 3 keeps its original crossover, output matrix, headroom and speaker
protections. No filter, gain, buffer capacity, SRC algorithm or clock calculation
was altered for this repair. The tested Stage 3 profile is retained in the 64k
bundle. Physical output still requires the validated profile and user's ARM.
Earlier 32k/r2 distributions are historical references, not this 64k live trial.

## Measured acceptance

- Stage 3 startup: known-data regression fails with old planner, passes with r3;
  six targeted checks pass.
- Decoder: full MSVC build and three targeted checks pass. Both PCM delivery
  owners use MMCSS Pro Audio / CRITICAL, retaining their existing timing policy.
- Publication maximum gaps: 8.8166 ms instrumented, 8.6162 ms installed, 8.5955 ms
  with the original GUI plus RAW trace; all below the unchanged 21.3333 ms target.
- Retained RAW captures are continuous at the exercised Windows warning; strict
  offline FFmpeg decoding passes. False warning-only resets are removed.
- Both DACs remain free of faults and DISARM. The three waits during the user's
  confirmed monitor switch are preserved as a real source transition; Stage 3
  retains ARM and resumes automatically. The user confirms correct DTS sound.

See [Cinema verdict](../validation/cinema-disarm-20261012/CINEMA-DISARM-VERDICT.md)
for exact counts, hashes, source-transition timestamps and remaining limits.
This qualifies the stated DTS intervals, not every source, AC-3 startup,
analog alignment, acoustic latency or the inherited pinned DSP limitations.

## Published material

`release/music-cinema/Stage3/0.1.9-v2-r3` contains the tested complete 64k runtime
and source archive. `release/music-cinema/Cinema/0.2.21` contains the decoder
runtime/installer, source bundle, retained RAW carrier and full trace evidence.
`release/music-cinema/Stage2/v0010` adds the current complete DLL runtime and
source overlay. Its unchanged native DSP source base remains explicitly in
`release/music-cinema/Stage2/stage2-music-v0009-source.zip` and
`Integration/Music/Stage2`. Stage 1 r9 remains in its already published source
tree and runtime/source ZIPs. SHA256 manifests accompany each new delivery.

Full telemetry is losslessly delta encoded to fit GitHub's file limit. Extract
the trace archive and run `python CompactTelemetry.py decode <capture>.jsonl.delta.gz
<capture>.jsonl` to reconstruct the original JSONL. Omitting the output filename
verifies its original SHA256 without creating another copy. Every captured row,
timestamp and field is retained. The original large ZIP and all RAW captures
also remain locally in `Diagnostics/Cinema-Disarm-20261012`.
