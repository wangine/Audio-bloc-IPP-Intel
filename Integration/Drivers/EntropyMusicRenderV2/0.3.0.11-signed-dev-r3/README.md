# Music Render V2 .11 — user-mode Provider revision r3

This bundle replaces only the user-mode Provider/audit artifacts. The already
installed SYS/INF/CAT and DEV signature are preserved; no kernel installation,
default endpoint, certificate or boot-policy change was made for this revision.

ZIP SHA256: `1EA49178DB280B8B4CF485FDF40BA89C3E02E4EF26D6BBAA016A909B99AD18E0`.
Manifest SHA256: `54EAE83831297D2E292297A0D74A6227416ED63B2CFCD5F3D10DE1EA8C0C8CE9`.
Raw Provider SHA256: `3329D657956A42A6E83C28850EE457D1A3A9BF9AC327955825621732B6C4B0DE`.
Signed kernel SHA256: `FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA`.

Use with [Stage1 v0003-r6](../../../Music/Stage1-v0003/releases/stage1-native-music-v0003-r6/README.md).
Optional capability 4 communicates confirmed IDLE/RUN on NoData only. Frozen
v0002 rejects that capability and must retain its old Provider. Raw PCM packet
bytes/layout and the frozen ABI header are unchanged. Unknown lifecycle metadata
does not retain idle suppression; RUN starvation remains detectable.

The manifest's `acceptance=NOT_RUN` refers to the preserved frozen-Core runner,
which cannot load this new capability. Paired r6 qualification is recorded
separately in its version-specific report and hash-bound evidence. Use the r6
diagnostic runner with explicit Core/SHA; do not interpret this bundle as a
frozen-v0002 qualification. Polling remains unsupported. V2 packet mode0 still
does not automatically certify WASAPI Shared/Exclusive access.
