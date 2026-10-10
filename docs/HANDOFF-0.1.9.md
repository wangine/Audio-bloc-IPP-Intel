# Referință pentru chatul următor — 0.1.9

Publicarea versiunii 0.1.9 este autorizată de utilizator la 2026-10-10, cu sursele, documentația, dovezile și ambele variante FIFO. Următoarea etapă va primi instrucțiuni într-un chat nou: separarea CINEMA de Stage1/2. Această separare **nu este implementată** în 0.1.9 și nu este proiectată în acest handoff.

## Punct de plecare

- Repository: `wangine/Audio-bloc-IPP-Intel`.
- Branch: `codex/headroom-cinema-overrange`; tag de distribuție: `v0.1.9`.
- Commitul surselor din binare: `293bc60c84c4b888095294b86dbb994e0d6c6a97`.
- Commitul dovezilor de packaging/smoke: `e027b08fabbb3184093783bb86cabcf1de9b719d`.
- Commitul indicat de tag include suplimentar acest handoff; nu schimbă binarele.
- Referința stabilă anterior acceptată: `v0.1.8`. Pachetele și tagurile existente nu sunt înlocuite.
- Folder local de distribuție: `D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release/0.1.9/`, subfoldere separate `EntropyAudioCore-0.1.9-32k-win-x64` și `EntropyAudioCore-0.1.9-64k-win-x64`.
- Worktree de dezvoltare: `D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/worktrees/crossover-thiran-delay`.

## Arhitectură de păstrat

Core-ul este proprietarul motorului/stării; Qt folosește numai IPC și telemetrie. Închiderea UI nu oprește motorul. Intrarea existentă FP64/192 kHz vine prin transportul pasiv compatibil; Core-ul nu controlează publisherul. Traseu: Crossover → Output Matrix → Headroom → FIFO per DAC → ClockSync per DAC → Guardian opțional → PCM24 → WASAPI. Fiecare DAC are domeniul său de clock. Nu modifica arhitectura, matematica DSP sau referințele stabile prin presupuneri despre viitoarea etapă.

## Ce schimbă 0.1.9

`Engine/AudioGraph.cpp`: analiza finală Headroom activată de host se execută pentru ambele programe; se elimină condiția exclusiv MUSIC. Detectorul vede blocul real post-Matrix/pre-gain, inclusiv crossfade-ul. AUTO/ATTACK/RELEASE/cap/force, MANUAL și BIT PERFECT rămân intacte. Crossover, Matrix, HeadroomEngine, ClockSync, Guardian, PCM24, WASAPI, Qt și Stage1/2 nu sunt schimbate în acest patch.

## Dovezi și limite

- PASS: build MSVC x64 19.51.36257.0 / IPP2026.0 static, `--probe`, targetul offline limitat `EntropyStage021Headroom`, pornire Core/Qt pe endpoint izolat. FIFO 32768/65536, aceeași țintă 4096 frames.
- PASS: diferențe MUSIC/CINEMA 0 pentru aceeași secvență și stare; MANUAL multiplicator exact; BIT PERFECT 24576 payload checks; un switch, 23 blocuri cu mix intermediar și coadă AUTO păstrată.
- FAIL OVERRANGE: fragmentul istoric Stage020 în MANUAL −3dB produce 68 samples / 1 bloc clipped pe DAC A. Pin1: CH1→D1, pin2: CH5→D2. Primul tap peste ±1 este Crossover. Peak maxim post-ASRC 1.0474431838795553, marjă observată lipsă 0.4026095dB. Nu este o constatare de clipping în sesiunea live actuală. Contoarele sunt DELTA; rail clamp și NaN/Inf sunt 0. Profilul/gain-ul nu au fost modificate.
- True peak: NOT IMPLEMENTED; Stage04: FAIL REALTIME; V07 formal: BLOCKED. Alinierea analogică A/B nu este măsurată; limitările software anterior raportate sunt păstrate.
- Nu declara o nouă acceptare live măsurată numai din autorizarea publicării. Pachetele sunt cele deja oferite utilizatorului, fără recompilare sau teste hardware suplimentare în timpul publicării.
- Rapoarte: `docs/STAGE020-TRANSFER-PROFILE-REPORT.md`, `docs/STAGE021-HEADROOM-CINEMA-REPORT.md`; dovezi `validation/stage020`, `validation/stage021`; instrucțiuni audit05 păstrate în `audit/2026-10-10-stage05`.
- Raw audio/taps/profilul privat rămân exclusiv în `local/stage020`, în afara Git și arhivelor de distribuție. Intel SDK, librării statice și directoare build sunt excluse.

## SHA256 Core

- 32k: `732F64F8D28CA597717B01F17D15869274A0B32A4C0FD347AF4B6850A80F6308`.
- 64k: `602CDF3D343AA45E5D66E1C97D334F6F34906A43488C0BF57630CBE9B209A775`.
- UI neschimbat: `254CC0DA52626DD351AF24FC3CEDC70C12456E5AD3F089A8CA37F304A190B400`.

Nu porni două Core-uri pe același endpoint. `Live-Test.cmd` pornește o sesiune explicită DISARMED; ARM este comandă separată. Nu începe etapa CINEMA până la instrucțiunile utilizatorului în chatul nou.
