# Stage2 native input integration

User approved real Stage2 input on 2026-10-09, with all physical DACs DISARMED.
Baseline: 8bb68f9, branch codex/stage06-stage2-ingress. Work in the existing checkout
as requested for Stage05; keep the running Stage05 pair untouched during builds.

1. Copy the original StageBus SDK and StageLink transport unchanged; record hashes.
2. RED/GREEN: original transport fixture exercises lease handoff, 11-lane FP64
   identity, signed zero, ordering, missing producer, starvation and recovery.
3. Native Engine/Input/Stage2Ingress worker owns connection, requests, heartbeat,
   release and a preallocated SPSC block queue. No transport calls in DSP processing.
4. The existing graph accepts 1024-frame Stage2 blocks; the two unused ABI inputs
   remain zero. Parameters still adopt only at block boundaries. DSP files unchanged.
5. Expose actual source state and queue counters through the existing IPC telemetry.
   Cold start selects real Stage2; offline fixtures stay explicitly separate.
6. Build MSVC/IPP into build/msvc-stage06; run complete native and Qt regression
   suites, then approved live input-only check. Preserve user state before restart.

User additions: UI exit leaves Core running with last applied state; cold start
reconnects to the same Core. A new real-input Core loads Documents/entropy.entropy
through the native profile validator. Missing/invalid profiles leave Core waiting,
with no Stage2 ownership claim. Qt offers Create / Load / Keep waiting via IPC.
New profiles use original defaults with explicit Manual Safe -6 dB and DISARMED
physical outputs. Offline fixtures never auto-load user Documents profiles.

No Stage2/Stage3 changes, alternative SRC, DSP changes, hardware ARM, push or merge.
Physical deadline and V07 gates retain their prior status. Final fresh-context review.
