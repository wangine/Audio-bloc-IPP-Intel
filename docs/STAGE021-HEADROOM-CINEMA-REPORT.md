# Etapa audit 05 — Headroom MUSIC/CINEMA, candidat 0.1.9

Corecția de integrare este **PASS**: analiza Headroom primește blocul real post-Matrix/pre-gain în MUSIC și CINEMA, inclusiv crossfade-ul emis. Fragmentul istoric reutilizat are **FAIL OVERRANGE** pe DAC A cu profilul său MANUAL −3 dB. Aceste rezultate sunt separate. Audiția candidatului 0.1.9 este **PENDING**; 0.1.8 rămâne referința acceptată.

## Modificare limitată

Baseline surse: `3eb55db371c6c4294a5010b6f704d2bbfdef0d12` (audit Stage020, după beta 0.1.8).
În `Engine/AudioGraph.cpp` se elimină exclusiv condiția `program == 0` din apelul existent `analyzeFinal`. Host-ul Core activează deja analiza. Numele vechi `enableOriginalMusicAnalysis` este păstrat pentru compatibilitatea host-urilor; comentariul explică acoperirea ambelor programe.

Detectorul masked global, scalarul comun, AUTO, ATTACK/RELEASE, limita AUTO și `force(safetyTarget)` sunt neschimbate. MANUAL și BIT PERFECT își păstrează politica. Nu sunt schimbate Crossover, Matrix, HeadroomEngine, ClockSync, Guardian, PCM24, WASAPI, IPC, Qt sau sursele Stage1/2. Nu este adăugat gain, limiter, profil ori clamp. True peak rămâne **NOT IMPLEMENTED**.

Cele 101 fișiere din manifestul protejat anterior sunt verificate SHA256; singura diferență permisă este integrarea `Engine/AudioGraph.cpp`. `validation/stage021/source-integrity.json` conține rezultatele și hash-urile. Referințele stabile și pachetele 0.1.8 rămân intacte.

## Verificare offline limitată

Un singur target: `EntropyStage021Headroom`, fără abonare la publisher și fără DAC. Configurație reală AudioGraph, routing 1:1 pentru aceiași trei lanes, același ledger și aceeași secvență de vârfuri în cele două programe. Sunt 32 blocuri de 256 frames per mod, vârful post-Matrix ajunge la 2.0; bufferul mut nu este folosit ca dovadă de succes.

| Verificare | Rezultat |
|---|---|
| AUTO: decizie/gain/țintă/peak MUSIC versus CINEMA | PASS, eroare absolută maximă 0 |
| Ieșire FP64 între cele două programe, toate cele 3 moduri | PASS, eroare absolută maximă 0 |
| MANUAL stabilizat: multiplicator `pow(10,-3/20)` | PASS, eroare absolută maximă 0 |
| BIT PERFECT: payload FP64, inclusiv semnul zero | PASS, 24.576 comparații bit-exact |
| Revizia aplicată și peak-ul blocului curent | PASS |
| Un switch cu surse diferite și coadă AUTO existentă | PASS, 1 eveniment; 23 blocuri cu mix intermediar |
| Gain înainte / după intervalul de coadă | 0,49999743782869926 / 0,49067179171191444 |

Inițial testul a eșuat pe integrarea veche: `current post-Matrix peak missing/stale in selected program`. Logul RED se păstrează. `prelink-old-binary.txt` păstrează o execuție prematură a aceluiași binar vechi înainte de terminarea linkării; nu este un defect suplimentar. Rezultatele GREEN sunt ale binarelor reconstruite după terminarea build-ului. Toleranțele nu au fost relaxate. Nu s-a rulat o suită completă sau benchmark.

## PCM: DELTA și primul tap

Se reutilizează fragmentul privat Stage020 de șase secunde și taps-urile sale existente. Nu s-a capturat un nou flux. Exportul aplicat indică **MANUAL, effectiveMode=2**, gain curent/țintă `0.7079457843841379`; valoarea −3 dB nu este tratată drept dovadă de gain pentru BIT PERFECT. În acest export gain-ul aplicat este verificat explicit. AUTO nu era modul reclamat în fragment.

PCM24 original, CLIP_AND_REPORT + TPDF, a procesat offline 1117 blocuri × 1024 frames per DAC; seed determinist de test, fără modificarea seed-ului de producție. Contoarele sunt diferențe înainte/după în această rulare, nu totaluri istorice copiate.

| Domeniu | FP64 overrange | Blocuri clipped | Rail clamp cuantizare/dither | NaN/Inf |
|---|---:|---:|---:|---:|
| DAC A | **68** | **1** | 0 | 0 |
| DAC B | 0 | 0 | 0 | 0 |

Verificarea exactității contoarelor este PASS; condiția de a nu avea overrange pe fragment este **FAIL**. `pcm-deltas-32k.json` și `pcm-deltas-64k.json` afișează separat ambele stări.

| Ieșire fizică | Sursă XO → destinație Matrix | Peak XO / Matrix | Peak Headroom | Peak post-ASRC | Samples overrange la PCM | Marjă observată lipsă |
|---|---|---:|---:|---:|---:|---:|
| DAC A pin 1 | CH1 → D1 | 1,4612631947769439 | 1,034495118618035 | 1,034505172301308 | 32 | 0,294653 dB |
| DAC A pin 2 | CH5 → D2 | 1,4795621855186922 | 1,04744981197214 | 1,0474431838795553 | 36 | 0,402609 dB |

Primul tap peste ±1 este **post-Crossover**: CH5 la graph frame 115067 (0,599307 s), CH1 la 119450 (0,622135 s). Matrix păstrează aceleași mostre. Depășirea rămasă după Headroom începe pe D2 la frame 1025819, pe D1 la 1025954. Primul eveniment post-ASRC este pin 2 la render frame 1025572, valoare `1.0025005437349797`; poziția graph estimată din traiectoria salvată este 1025818,836. Indicii graph și render au origini/clock diferite și nu se compară direct.

Prin urmare, ASRC nu este originea acestei depășiri: pre-ASRC este deja peste ±1. Numărul samples poate diferi după resampling. Marja din tabel este `20*log10(peak post-ASRC)` pe acest fragment, **nu** o garanție pentru orice semnal sau true peak. Headroom aplică exact gain-ul profilului. Nu se schimbă automat profilul și nu se inventează o atenuare nouă. Contoarele și politica PCM rămân vizibile și intacte.

Acestea sunt dovezi offline pe date istorice, cu traiectoriile de clock Stage020. Nu demonstrează clipping în sesiunea live actuală. Raw audio, profilul privat și taps-urile rămân în `local/stage020`, excluse din pachet și Git; JSON-urile publicabile conțin numai măsurători.

## Build, variante și reproducere

MSVC x64 `19.51.36257.0`, Intel IPP static `2026.0`. Fără AVX2 obligatoriu. Pachete separate: FIFO 32.768 / 65.536 frames; aceeași țintă **4096 frames**, aceeași UI și aceeași corecție. Creșterea capacității nu crește ținta de latență.

```powershell
tools/Build-Stage021.ps1 -FifoFrames 32768 -Verify
tools/Build-Stage021.ps1 -FifoFrames 65536 -Verify
# Opțional: numai dacă există deja fragmentul/taps-urile private Stage020:
tools/Build-Stage021.ps1 -FifoFrames 32768 -Verify `
  -HistoricalProfile local/stage020/ACTIVE-original-copy.entropy `
  -HistoricalTaps local/stage020/replay
python tools/Analyze-Stage021-Overrange.py local/stage020/replay `
  local/stage021/build-32768/results/pcm-deltas.json `
  local/stage021/overrange-attribution.json
```

Python pentru atribuire necesită NumPy; nu importă sau modifică DSP. `--probe` verifică pregătirea Core fără hardware/IPC. Pachetele sunt verificate cu smoke Core/Qt pe endpoint offline unic; nu se oprește Core-ul live și nu se armează ieșiri.

Review read-only: fără constatări importante asupra corecției și semanticii contoarelor. Testul final a fost întărit cu două surse distincte la crossfade și statut overrange separat.

Limitele istorice rămân vizibile: Stage04 **FAIL REALTIME**, acceptarea formală V07 **BLOCKED**; alinierea software A/B raportată anterior nu este verificată analogic. Acest patch nu le recertifică și nu autorizează FIR8193 ×16 pentru live.
