# Live renderer implementation plan

Goal: deliver the authorized live Stage3 candidate with the existing UI, DSP and exact saved device routing.
Spec: RendererLive/DESIGN.md. Implementation owner: ChatGPT; local Codex builds/packages only.

1. Add failing bounded contracts for frame queues, ready-packet commit and total-backlog rate control. Add a standalone native verification target, independent of IPP and hardware.
2. Implement portable preallocated queues, slow ratio control and the pinned FP64 WDL adapter with constant sinc bank. Verify ordering, packetization, anti-windup and no phase reset on rate changes.
3. Implement per-domain preparation/PCM24 and a small independent event-driven WASAPI backend. The sink only copies prepared bytes and commits queue ownership after successful ReleaseBuffer.
4. Replace LiveSinkWorkers behind the unchanged Core/UI interface. Preserve graph retirement handshake; make endpoint preflight and failures independent. Source outages use the existing lifecycle, not a new upstream controller.
5. Build/package script: exact revision, isolated worktree/build/runtime, unchanged UI by hash, copied saved profile, no test signals or automatic ARM. Preserve all old runtime folders.
6. Review compile/type/lifetime/routing paths; record verification actually executed and limitations. Only then publish the final handoff SHA.

Review focus: cancellation during open/prefill; secondary failure while primary runs; source gap or buffer overflow without invented PCM; failed submit without advancing a packet; profile/DAC changes only after worker retirement. No local diagnostic task or mapping audit is added.
