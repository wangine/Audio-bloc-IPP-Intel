# Entropy AudioCore 0.0.4 — Thiran beta test

Experimental local candidate, separate from stable beta 0.0.3. Stable 0.0.3
remains the reference until the next version is accepted by listening.
See `docs/THIRAN-VALIDATION-20261009.md` for measured results and scope.

18 focused native/control tests PASS; Core/Qt IPC test PASS; Linear reference
bit-identical to 0.0.3. Thiran 1/3/5 retain steady-state amplitude, with order
dependent phase error. Formal Stage04 FAIL REALTIME / V07 BLOCKED remain.
No new hardware execution or GitHub publication is claimed.

## Listening

1. Save your current profile, close the current Qt and stop that Core with its
   own Stop-AudioCore.cmd. Keep Stage1/Stage2 running. This candidate refuses to
   reconnect to another version's Core on the same endpoint.
2. Launch **Live-Test.cmd in this 0.0.4 folder**. Core starts DISARMED.
3. Load your own profile and DAC assignments. In Crossover → ROUTING, select
   the output and FRACTIONAL DELAY → **Thiran 5** for the initial audition.
   Repeat for the wanted outputs, or use the existing APPLY ALL L → R command
   for paired ways. The displayed effective order comes from Core telemetry.
   Short delays reduce the order causally; exact integers use ring bypass.
4. ARM explicitly when ready. A/B with **Linear (reference)**. Original
   profiles initially select Linear, so loading one alone does not enable
   Thiran. Save the trial profile under a new name to keep the reference.
5. Closing Qt leaves Core running. DISARM/stop this version before returning
   to 0.0.3. Do not mix runtime files between version folders.

Global Base is still present. Complete removal from UI/engine and profile
migration are deferred until the audio test is accepted. Headroom V2 and all
other DSP/Egress mathematics remain unchanged. FIR8193 ×16 remains forbidden
live; no new limiter, attenuation or normalization was introduced.

## Hashes

- Core SHA256: `FB800425171F5A7962FF7434B08819D39C8FD44FD879FFEBD93AFAA17132410B`.
- UI SHA256: `A777E0B537DE6DB8E2528B5D37DD01C42B86FFA04C6D15D98435C7A0A10D60AC`.

The manifest identifies the source commit and payload hashes. This folder
contains its own launchers, Core, IPC-only Qt UI and runtime dependencies.
