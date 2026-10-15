# ClockFollow C2 — IMPLEMENTATION AND ISOLATED BUILD

Read `ClockFollow/CODEX-C2-INTEGRATION.md`. This branch initially contains the implementation task/specification only; C2 is NOT already implemented or audio-qualified by that documentation commit.

When the owner gives Codex this task, implement the specified MUSIC/AUX clock integration and build an isolated candidate. This replaces the earlier compile-only assignment for this C2 worktree only. Earlier C1/LiveV1.1 instructions must not prevent authorized source changes in C2, nor authorize their old deployment tasks.

Use one isolated worktree at `D:\Projects\Entropy-ClockFollow-C2-20261015\source`, on `chatgpt/clockfollow-c2-integration-20261015`, initially based on the exact handoff SHA. Preserve all original checkouts, C1/B1/Live packages, active applications, driver installations and profiles. Verify Git with explicit paths; no reset, clean, stash, force or arbitrary substitution of a copy on another drive.

Application base is `2264e5efb62e411a83432a2b41b9b6c5bde5df12`. Import only necessary C1 sources/contracts from `1faae8979f03c32baf61861b921fbe0a46e64e51` and AUX from the pinned A06 source in the task. Do not merge an old entire engine. The installed AUX identity is already provided; source-to-installed-binary equivalence is UNPROVEN. Do not repeat discovery or manufacture provenance.

Implement actual call paths, not just declarations, models or an unconnected helper. Preserve DSP algorithms, r8brain, PCM24, gain modes and speaker assignments. Stage2 DSP is unchanged; minimal versioned IPC metadata forwarding is permitted only if necessary and isolated. Keep the existing compatibility renderer, with WDL for asynchronous CINEMA and the secondary DAC. Do not remove WDL until DIRECT_PRIMARY has a coherent synchronized source contract. No silent dual rate-control loop, source drop/replay or phase jump to hide starvation.

Codex may fix its own scoped implementation/compiler/test errors. The historical rule to stop at the first compile error applies to compile-only handoffs, not to writing the C2 implementation. Do stop for missing permissions/required source, an architectural deviation or any change outside this approved scope. Do not suppress warnings/tests to manufacture success.

Use existing toolchains and focused offline contracts in the task. No new broad diagnostic campaign, REW/Tidal sessions, PCM captures, mapping reports or repeated historical tests. No toolchain installations, global FP/AVX2 policy change or unrelated optimization.

Build/package only, without physical-device interaction: no signing, certificates/keys, driver installation, PnP/registry/boot-policy/default-audio changes, services, reboots, audio process start/stop, DAC access or ARM. Existing C1 candidate stays unsigned and untouched. Deployment and loaded-driver/audio qualification require a later explicit owner decision.

Produce source changes, local commits, build/package results and an honest per-component status. Do not call the instruction commit a new renderer or a model PASS a complete live qualification. No push/merge unless the owner requests it. The final C2 candidate must state that installation and physical audio validation have not been performed.
