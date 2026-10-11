# Stage2 MUSIC v0009 — livrare locală

Data: 2026-10-10. Contractul Stage2 a fost separat la **2ch IN / 3ch OUT**,
FP64 planar / 192000 Hz / 1024 frames, păstrând DLL-urile și rutarea DSP.

## Livrare

- Copie de lucru: `D:/Projects/3Stage-3/Stage2`.
- Runtime candidat: `release/stage2-music-v0009`.
- Launcher: `release/stage2-music-v0009/ColdStart-DSP-Music.cmd`.
- Contracte: MUSIC_STEREO2_V1 (ID 3), MUSIC_LRC3_V1 (ID 4).
- Intrări: MUSIC_L, MUSIC_R. Ieșiri: MUSIC_L, MUSIC_R, MUSIC_CX.
- Core SHA256: `A9E8F777A870185152F3DF92DE410C02FF54486363AFFCE7F3877CC83572D3F3`.
- Contract MUSIC SHA256: `8d1dd0d0b733489a83ebb5a84ed287ad211175d27d7fbea64fa969130f55318e`.

Source baseline: arhiva 0.1.9 distribuită,
SHA256 `DAD485108CB12273F48104294B4AF688099A356E45AEEAA55AC87AE99A5957FC`.
Sursele EQ/Center/LRC/Alignment au fost comparate cu această arhivă și sunt
identice. Inventar comparativ: `source-provenance.json`.
Toolchain: MinGW x64 13.1.0, Qt 6.11.2, C++20, CMake/Ninja.

## Schimbări și independență

Stage2 refuză publisherii legacy 10ch; nu trunchiază implicit contractul vechi.
Publică numai L/R/CX, fără plane ori metadata CINEMA în contractul MUSIC.
EQ procesează L/R; Center primește tap-ul L/R brut; LRC Align procesează L/R/CX.
Loudness păstrează BYPASS / INACTIVE din baseline; nu a fost activat sau rescris.

Cele cinci DLL-uri sunt byte-identice cu manifestul v0008: Center original,
Center V2, Loudness, Parametric EQ și LRC Aligner. Executabilul din pachet este
identic cu build-ul verificat; `--verify-only` verifică ambele contracte SDK,
numărul 2/3 de canale, manifestul și hash-urile modulelor.

Publisherul/subscriberul folosesc endpoint-uri MUSIC distincte și mutex de
ownership distinct. Calendarul, sesiunea, UUID-ul, epoch, cursorul și semantica
metadata MUSIC sunt păstrate. Un abonat lent sau oprit nu comandă sursa.
Queue-ul V2 păstrează ABI-ul/storage-ul maxim existent; copiază strict numărul
de plane declarat în noua sesiune, fără semantica unor canale CINEMA.

UI-ul are pin-uri 2/3 și nu mai are carduri/direct-wire CINEMA. Pagini și controale
native păstrate. QA vizual offline: `music-ui.png`, cu font și stil existente.
Recorderul MUSIC3 are 24704 bytes/bloc (128-byte header + trei plane de 8192 bytes),
format distinct de capturile istorice 11ch; vezi README.

## Rezultate verificate

| Verificare | Rezultat |
| --- | --- |
| Build complet | PASS |
| MUSIC Contract/Passive/Recorder/UI/Native/Allocation/Package/Process/RecorderLifecycle | **9/9 PASS** |
| Funcționale păstrate: sesiuni StageBus, persistență, EQ/Center native și calcul Alignment | **9/9 PASS** |
| Core din pachet `--verify-only` | PASS |
| GUI din pachet `--smoke-test`, offscreen, exit code așteptat explicit | PASS, exit 0 |
| Process fixture: executabil real + native DLL + IPC + stop/restart | PASS |
| Payload bypass: signed-zero, NaN payload, LR bit-exact, CX positive zero | PASS |
| DSP diferențial: aceeași DLL EQ, raw Center, LRC activ/bypass | PASS bit-exact |
| 1000 cicluri audio încălzite: host new/delete + import audit alloc/free/wait/lock/I/O | ZERO în limitele auditului |
| SDK/session hashes, malformed channels/stride, metadata CINEMA refuzată | PASS |

Fișiere: `music-green-final.log`, `retained-functional-green.log`,
`music-allocation.log`, `package-verification.log`, `package-gui-smoke-result.txt`.
RED observat înainte de schimbarea codului: `music-red.log`, `native-red.log`,
`package-red.log`. Un loop de 11 plane rămas în fixture-ul suplimentar de recorder
a fost găsit la review și corectat numai în test; RecorderLifecycle trece acum.

Auditul nu acoperă toate syscall-urile, rezolvarea dinamică sau module încărcate
după instalarea hook-urilor. Nu reprezintă o nouă calificare realtime sau măsurare
de CPU/latency pe hardware.

## Suitele istorice — rezultate păstrate explicit

Baseline v0008: **21/44 PASS, 23 FAIL**, reproduse cu token Windows normal.
Candidatul cu toate fixture-urile istorice și MUSIC: **22/53 PASS, 31 FAIL**.
Nu se declară că întregul CTest este verde.

Cele 23 eșecuri baseline rămân în suitele V1/control/UI istorice incompatibile cu
lifecycle-ul pasiv: DspBusIntegration, DspLiveness, DspAllocation, DspLifecycle,
DspProcess, DspPendingFailure, DspRestartProcess, DspPeerDeathPending,
DspSourceIsolation, DspPublishedOutput, DspCenterRoute, DspCenterController,
DspCenterUi, DspCenterAllocation, DspCenterProcess, DspLrcRoute, DspLrcController,
DspLrcUi, DspLrcAllocation, DspProfileUi, DspEqRoute, DspEqUi, DspEqAllocation.

Cele opt eșecuri suplimentare sunt așteptări legate explicit de vechiul contract:
DspBypassIdentity, DspRecorder, DspPackage, DspPassiveInput, DspPassiveNative,
DspPassiveAllocation, DspSignalTelemetry, DspGraph. Aceste teste au fost păstrate;
noul comportament este verificat de fixture-urile MUSIC separate de mai sus.
Detalii nominale: `baseline-ctest-normal-token.log`, `full-ctest-final.log`.

## Limită de integrare

Stage1 actual publică 10ch; Stage3 0.1.9 citește 11ch. Acest candidat MUSIC **nu
este direct compatibil cu ele**. Nu au fost implementate adaptoare de tranziție,
selectorul Stage3 ori SRC-ul CINEMA din decodor. Testele au folosit exclusiv surse
sintetice și endpoint-uri izolate; nu s-au conectat DAC-uri sau surse fizice.

Stage1 a rămas pentru explicațiile suplimentare ale utilizatorului. Stage3,
decodorul, versiunea Stage2 activă și profilurile lor nu au fost modificate de
această implementare. Candidatul are state separat și pornește cu defaults
existente dacă nu i se oferă un profil propriu; nu copiază editări live nesalvate.
Sursele și livrarea rămân locale; nu s-a făcut push/merge/publicare.
