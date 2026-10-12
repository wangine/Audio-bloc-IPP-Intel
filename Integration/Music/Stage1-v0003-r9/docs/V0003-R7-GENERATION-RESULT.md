# Stage1 v0003-r7: lifecycle ownership across generations

2026-10-11. Stage1 native Core only. Signed kernel 0.3.0.11 and paired raw Provider bundle r3 are unchanged. Stage2/Stage3 DSP are unchanged.

## Confirmed defect and regression

The r6 real TIDAL capture contains 433 source samples over 179.594 seconds, including generations 163/165/167. MUSIC underruns increased 47→62 (+8 and +7 at two transitions), while steady RUN intervals remained stable. Source fault/lost/overrun, publisher-drop and clock-late counters remained zero. Driver lifetime lost and write failures remained zero; intentional STOP cancellation increased 6,144 frames and is retained. This is a counter/stream-lifecycle observation, not a PCM byte comparison or lossless end-of-stream qualification.

Provider `read()==0` already carries the generation returned by driver INFO. Core r6 ignored that generation when applying RUN to its previously prepared SourceDomain. A new stream could therefore start the old domain's resume deadline before its own first packet arrived. The actual Core synthetic regression reproduces this failure: old-generation PCM → IDLE → new-generation RUN without PCM → first new-generation PCM → genuine starvation in the new generation. `evidence/r7-generation-red.txt` preserves the failure; `r7-generation-green.txt` records all six CTest cases passing after the fix.

Core r7 applies known RUN only to the generation prepared by that ingress worker. The old domain drains its existing complete PCM blocks as IDLE while awaiting the new stream's first packet. That first packet performs the existing prepare/SRC startup. Unspecified generation 0 preserves the prior RUN behavior; UNKNOWN still restores conservative starvation accounting. A RUN stream that already supplied PCM still counts starvation. No PCM, SRC, gain, FIFO size or prime-window changes are introduced.

## Real playback validation

The active r6 process was gracefully replaced by a fresh r7 process. Its accumulated counters were preserved in `evidence/tidal-v2-real-20261011/r6-before-r7-restart.json` (MUSIC underruns 69 at that snapshot). The new process has its own counter lifetime; no r6 counters were cleared to claim a pass.

r7 real playback capture and final bounded result are recorded under `evidence/tidal-v2-real-20261011/`: `r7-real-transition-source.jsonl`, `r7-real-transition-driver.csv`, and `R7-ACTUAL-TRANSITIONS.json`. The user confirmed three pause/play cycles during this live capture; generations 169→171→173→175 delimit those cycles. Subsequent stream/format transitions are retained separately in the report. Use the final JSON for duration, frame totals, all generations, unavailable INFO samples and counters.

The PCM identity qualification of r6 remains historical evidence for its exact binaries. It is not relabeled as an r7 live identity audit. r7 is verified by its six Core tests and real lifecycle counters; the raw Provider binary and PCM/SRC implementation are unchanged. Known-pattern tools remain available for later complete transport/output comparisons.

## Exact artifacts and remaining limits

- r7 Core SHA256: `21FEFFBC6E3F4CAAF2AB58CB1FB5C272641C04672494420D4BBD9C13DFCE3A7E`.
- Raw Provider SHA256: `3329D657956A42A6E83C28850EE457D1A3A9BF9AC327955825621732B6C4B0DE`.
- Qt SHA256: `4EAF91729BF85D739D554BBADBB72BBF142E394B4BDF6DE0F060B05078D02B85`.
- Signed kernel SHA256: `FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA`.

V2's actual WASAPI Shared/Exclusive access remains unverified in its metadata; the UI explicitly reports this. User-confirmed TIDAL selection is not automatic driver proof. Spotify remained paused, but brief nonzero AUX signal was observed during the capture; AUX counters are not a new sustained non-silent AUX qualification. Earlier AUX stereo findings remain unresolved. Finite sampling cannot establish every transient or PCM byte identity. Driver INFO busy samples must not be mistaken for a stopped stream; the summarizer excludes unavailable INFO from state transitions and retains those errors separately.

Start the current runtime using `release/stage1-native-music-v0003-r7/Start-Stage1-Music.cmd`. Immutable r6 and v0002 references remain available. Frozen v0002 must retain its original Provider bundle.
