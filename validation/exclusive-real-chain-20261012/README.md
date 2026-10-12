# Live Exclusive reference, 2026-10-12

The known 24/96 diagnostic signal and the saved listening profile were checked through Stage1 r9, Stage2 v0009, Stage3 r2 64k and both active DAC sinks. The user confirmed correct playback after the original Stage3 Core was restored. Listening to additional music remains open. No audio correction was implemented.

Read [the verdict](REAL-CHAIN-VERDICT.md) for numerical results, the AUX metadata transition before audible onset, startup comparison exceptions and the unverified physical A/B alignment. These limitations are part of the result.

## Production references

| Component | Complete source | Complete runtime |
|---|---|---|
| Stage1 v0003-r9 | `Integration/Music/Stage1-v0003-r9`, `release/music-cinema/Stage1/v0003-r9/*-source.zip` | `release/music-cinema/Stage1/v0003-r9/*-runtime.zip` |
| Stage2 MUSIC v0009 | `Integration/Music/Stage2`, existing Stage2 source ZIP | `release/music-cinema/Stage2/stage2-music-v0009-win-x64.zip` |
| Stage3 0.1.9-v2-r2 | Production snapshot at commit `fd735c5420d81f4ec89920d56c9a4039ce8809e7`, existing source ZIP | Existing 32k/64k archives in `release/music-cinema/Stage3/0.1.9-v2-r2` |

The current Stage3 source adds optional observation taps and offline tools. Existing Stage3 production executables were not replaced. Stage1 r9 is imported as a frozen source/runtime reference, including its required legacy bridge DLL dependencies. Historical notes inside that snapshot describe its earlier local validation; this publication is the later handoff.

Local launchers, independent of startup order:

- `D:\Projects\3Stage-3\Stage1-v0003-r9\release\stage1-native-music-v0003-r9\Start-Stage1-Music.cmd`
- `D:\Projects\3Stage-3\Stage2\release\stage2-music-v0009\ColdStart-DSP-Music.cmd`
- `D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\release\0.1.9-v2-r2\EntropyAudioCore-0.1.9-v2-r2-64k-win-x64\Live-Test.cmd`

`reference-manifest.json` records archive and executable hashes; the archived Stage2 and Stage3 production executables were checked against the unchanged live reference hashes. `capture-sha256.json` identifies the full qualified PCM captures preserved locally. Large captures are not in Git.

## Audit tools and provenance

`release/music-cinema/Diagnostics/exclusive-real-chain-20261012-tools.zip` contains the passive subscriber, offline graph replay, offline DAC reference checker, analysis scripts and the live-qualified observation Core. It is an explicit diagnostic delivery, not the production runtime. The Python orchestration scripts retain the original machine paths and require reviewing those paths before reuse.

The packaged observation Core produced the qualified capture. Final source additionally checks worker flush/close failures and rejects a nonempty capture directory; that hardened Core was compiled, but not subjected to another live run. The final offline readers were rerun against the complete qualified capture. Targeted integrity checks rejected missing, truncated and nonfinite records, and exercised restoration failure paths with mocks: 9 checks passed. These are diagnostic-tool checks, not a general audio test suite.

Graph replay uses the same native processing engines and identical profile, not an independently designed filter reference. ClockSync reconstruction checks every eighth sample on all channels. Final PCM verification checks every submitted audible sample against the existing PCM24 TPDF bounds, with explicitly documented inferred startup-prefix exceptions. Do not claim stronger certification than these checks provide.

The observer is enabled only by `ENTROPY_CHAIN_AUDIT_DIR`; audio-thread work is bounded copying into preallocated queues. Disk I/O runs on workers. Preserve the crossover and protection profile when repeating a live observation. Do not run `ObserveChain.py` casually: it temporarily replaces Stage3 Core, then restores production, and needs exclusive access to its control endpoint.
