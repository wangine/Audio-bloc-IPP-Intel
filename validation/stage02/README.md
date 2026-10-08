# Stage02 published evidence

**Stage02 = PASS NUMERIC / FAIL REALTIME. Formal V07 acceptance = BLOCKED.**

These are executed offline measurements, not production approval. See the
[complete report](../../docs/VALIDATION-STAGE02.md) and
[CPU tables](../../docs/BENCHMARK-STAGE02.md) for methods, thresholds and limitations.

| Evidence | Coverage / result |
| --- | --- |
| `numerical-complete.csv` | 2,128 distinct cases × 16 outputs × native/V07 = 68,096 rows. Native absolute/RMS error, phase difference and impulse lag are zero. |
| `parameters-complete.csv` | 17,920 actual native/V07 parameter values across 112 configurations. |
| `controls.csv`, `delay-impulses.csv` | 6,912 gain/delay/amplitude/phase checks and 768 delay impulses, all PASS. |
| `transitions.csv`, `transition-windows.csv` | 16 event sequences; 256 native and 256 V07 metric rows; 4,096 activity/silence windows PASS. |
| `benchmark.csv` | 224 module/program/configuration/block rows: 85 PASS, 139 FAIL against observed callback maximum/budget. Raw QPC median, nearest-rank p99 and maximum in microseconds. |
| `realtime.csv` | 240 scenarios, 497,040 callbacks: no tracked alloc/free, waits, locks or filesystem I/O. Large callback resets remain FAIL; hook coverage is bounded. |
| `identity.csv` | Headroom bit identity PASS. Matrix arithmetic identity PASS, strict signed-zero bit identity FAIL in eight block sizes. |
| `summary.json`, `final-exit-codes.json` | Aggregate measurements and explicit per-check outcomes. |
| `source-provenance.json` | Pinned references: 12 files text-identical, 11 byte-identical; ProgramEpochTrace.h differs only by pre-existing CRLF/LF checkout endings. |
| `evidence/*.txt` | Executed build, compiler, CTest, probe, numerical, controls, transition, CPU and source-audit logs. |

V07 was available and compared: maximum absolute error 2.016182359954044e-15,
maximum RMS error 4.73243431495252e-16. All comparisons are within the reported
native tolerance. The V07 `BLOCKED` status concerns formal acceptance thresholds
for the expanded corpus; it does not mean the comparison was skipped.

The numerical corpus combines 741 completed initial cases and 1,387 completed
cases from eight independent offline processes. Each graph has one processing
thread. Benchmarking was serial after those processes finished: 54 initial rows
plus 170 resumed rows, with 200 measured transition adoptions in both lots.
Complete-case and duplicate checks run before merging. Saved CSVs are aggregate
metrics, not per-sample waveform dumps; no measurements are fabricated or dropped.

Reproduce on Windows with locally installed MSVC x64, CMake, Ninja and Intel IPP:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Run-Stage02.ps1
python .\tools\Summarize-Stage02.py .\build\msvc-release\stage02-results
```

Reference acquisition uses the pinned private upstream repository. Missing or
unverified V07 blocks only its comparisons; native checks continue. The runner
collects the remaining checks after FAIL/BLOCKED and deliberately returns nonzero.
It does not launch hardware, Stage2, Qt or IPC. Existing DSP sources are immutable.

`SHA256SUMS.csv` records hashes of this published evidence. The full raw-result
archive (including intermediate lots and measured executables) remains local in
the Git-ignored `local/stage02-archives/stage02-results-2026-10-08.zip`; SHA256:
`23F7033A6FDE9A773A216F0C4CB9B5FE7F06178CD8505EB3F9D6D8BC62C30B24`.
No executables, Intel libraries, raw archive or build directories are committed.
