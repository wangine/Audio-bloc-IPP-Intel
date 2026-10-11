# Permanent PCM and FP64 acceptance tool

Keep `EntropyMusicPcmAudit.exe`, both Provider DLLs, all source/tests, each versioned
package/manifest and captures. The tool is part of the private architecture branch;
it is not disposable installation scratch. No signing private key is included.

The original-to-Core comparison checks every PCM container byte, packet sequence,
generation and source frame from0 to the complete emitted tail. It requires the
exact isolated frozen Core PID, a drained stream and a clean observer footer.
Startup/tail trimming, shifted pattern search and hidden gain are prohibited.
Failures are preserved and return nonzero. `-ContinueOnFailure` records all cases
while retaining a nonzero aggregate result if any case fails.

The optional `-VerifyFp64` subscriber attaches to that Core's separate passive
MUSIC publication before rendering. It is not another raw driver reader. It
captures all publication blocks, including invalid clock blocks, and compares
every valid L/R binary64 sample to the exact known original PCM value. It rejects
publication gaps, altered values (including one ULP), generation changes, interior
discontinuities, inserted invalid/silent clock blocks inside the expected source
sequence, AUX, missing first/tail data and absent/error footers. Invalid clock
frames before the source starts and after its complete tail are counted explicitly.
Intentional PAUSE tests run separately from uninterrupted FP64 continuity tests.

**Direct equality is supported only for native192k input and output.** PCM16 is
sample/32768; PCM24-valid-in32 is (container>>8)/8388608. These powers-of-two
conversions are exact in FP64. Tests use even durations so the full emitted source
tail fits Stage1's immutable1024-frame publication boundary; no remainder is
discarded. Core/SRC/matrix/ABI are not modified for the test.

At44.1/48/88.2/96/176.4k the raw PCM test remains valid, but post-SRC sample equality
to the original is mathematically inappropriate. An independent, specified
r8brain reference, including latency/startup/flush, must be added before that
post-SRC comparison is claimed. Sample Peak, True Peak and SRC Startup Peak remain
separate diagnostics; none substitutes for sample identity.

Examples after verifying the installed signed package (absolute paths are required
for `$package`, `$repository` and each fresh `$output`):

```powershell
& "$package/tools/Run-Live-Acceptance.ps1" -Package $package -Repository $repository `
  -RunLive -ContinueOnFailure -Seconds 10 -Output $output
```

```powershell
& "$package/tools/Run-Live-Acceptance.ps1" -Package $package -Repository $repository `
  -RunLive -Servicing event -VerifyFp64 -Seconds 10 -Output $output
```

```powershell
& "$package/tools/Run-Live-Acceptance.ps1" -Package $package -Repository $repository `
  -RunLive -Servicing event -Seconds 2 -PauseMs 200 -Output $output
```

`-PeriodFrames` requests a different WASAPI period; actual buffer size/alignment is
recorded and unsupported requests fail explicitly. `-Stress` selects1800seconds
PCM24/192k Event-Driven. The default invocation opens no audio. All live commands
use package hashes and the exact new endpoint identity, not a friendly name or
system default. They stop only their own isolated processes.

Acceptance reports must name source/kernel revisions and package hashes. A raw
transport PASS does not prove FP64 output identity; an FP64 PASS at192k does not
qualify SRC at other rates or establish identical TIDAL PCM across Windows modes.
