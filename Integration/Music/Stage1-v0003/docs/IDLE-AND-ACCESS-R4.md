# Stage1 v0003-r4: upstream idle and access reporting

The r3 real TIDAL pause/play capture revealed that the driver had no running
stream while the Core continued counting missing scheduler blocks as underruns.
The original 180 s acceptance windows remain valid only for those windows.
Later transitions and cumulative counters are preserved separately.

The new user-mode Provider advertises capability 4 explicitly. On NoData only,
flag 16 and reserved[0] distinguish confirmed IDLE from RUNNING. A busy INFO query
remains unconfirmed. PCM packet layout, raw bytes, kernel .11, decoding, SRC,
gain and FIFO contents are unchanged. The scheduler drains completed audio during
IDLE and does not count expected silence as starvation. Resumption uses the
existing priming duration. RUNNING or unconfirmed status with no PCM counts
starvation after that window; unconfirmed status cannot retain idle suppression.

Candidate registry accepts the capability. Frozen v0002 rejects it and must keep
its original Provider. No old release has been overwritten. The ABI header and
struct sizes remain unchanged; the extension lives in a separate header.

Regression tests reproduce registry rejection, empty resumed RUN starvation,
and Idle→unconfirmed starvation in the real isolated Core, then verify the fixes.
Queued PCM drains unchanged and genuine RUN gaps still increment counters.
The actual updated raw Provider DLL is also loaded through candidate discovery.

## Access mode

AUX Windows Capture owns its WASAPI client and reports its configured mode.
TIDAL owns MUSIC WASAPI. Current kernel packet mode remains 0: it does not
transmit a verified Shared/Exclusive mode. Qt now reports
`Raw PCM · WASAPI access unverified`, rather than guessing from event notifications,
the selected private transport or the raw PCM flag. A user selecting Exclusive in
TIDAL is not equivalent to automatic confirmation through this metadata field.

Microsoft distinguishes [shared/exclusive user-mode paths](https://learn.microsoft.com/en-us/windows/win32/coreaudio/user-mode-audio-components)
from [signal processing modes](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-signal-processing-modes).
RAW processing mode is not proof of Exclusive access. Automatic mode reporting
requires a separately verified metadata source; it is not implemented here.

Live candidate results are recorded separately and must be read with their
duration, binary hashes, known-pattern scope and startup/stop boundaries.
