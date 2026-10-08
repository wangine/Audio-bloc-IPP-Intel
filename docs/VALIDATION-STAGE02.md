# Etapa 02 — rezultate executate, 2026-10-08

**Stage02 = PASS NUMERIC / FAIL REALTIME. Acceptarea formală V07 = BLOCKED.**
Sursele și rezultatele relevante sunt publicate exclusiv pe branch-ul Stage02,
cu aprobarea explicită ulterioară a utilizatorului; fără merge în main.
Datele publicate sunt în [validation/stage02](../validation/stage02/README.md).

Raport offline pentru branch `codex/stage02-fp64-validation`, bază
`019e3aa04cbe8e5f4c405568ee09f971a4a1e3cd`. Rezultatul general este **FAIL**:
identitatea bit-cu-bit a Output Matrix nu păstrează semnul lui zero, iar
Crossover păstrează resetările masive ale bufferelor în callback. Aceste probleme
au fost măsurate/identificate și nu au fost reparate prin schimbarea DSP-ului.

## Build și integritate

- MSVC x64 19.51.36257.0, `_MSC_FULL_VER=195136257`, toolset 14.51.36231;
  Visual Studio Community 2026 18.10.0; C++20, Release, `/fp:precise`.
- CMake 3.30.5, Ninja 1.12.1; `tools/Build-Windows.ps1 -Stage02 -CompileOnly`.
- Intel IPP static 2026.0.0 `(r0xa7ad6ebc)`, build Mar 27 2026,
  `C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0`.
- Intel Core i7-9700F, 8 cores / 8 logical processors, 16 GB RAM, Windows;
  fixed 192 kHz, FP64, 13 inputs / 16 outputs, un singur thread de graph per test.
- **PASS**: toate cele 12 fișiere DSP de implementare/header au conținut textual
  identic cu referințele pin-uite; 11 sunt identice byte-cu-byte.
  `ProgramEpochTrace.h` diferă numai prin terminatoarele de linie, deja existente
  în checkout, fără schimbare de cod. Diff gol pentru `DSP`, `Shared`, `Egress`
  față de bază.
- **PASS**: cele patru binare Stage3 care aveau modificări preexistente au
  aceleași SHA256 observate înainte/după această etapă. Nu au fost pornite DAC-uri,
  Stage2, Qt, hardware output sau IPC. Push-ul ulterior autorizat este numai pe
  branch-ul Stage02; nu se face merge în main.
- Schimbările de producție sunt două metode AudioGraph pentru configurare offline
  în repaus, care folosesc setter-ele și pregătirea originală. Procesarea AudioGraph
  este nemodificată. Restul schimbărilor sunt harness, CMake și scripturi.

Referințe: Crossover IPP `99b84d13a6808486690e24feba35128d7675be95`,
Output Matrix / Headroom `6cbac37233c8094e39828de8309a9fe13d724ad4`.
Referințele native sunt compilate separat, din sursele originale extrase,
cu remaparea namespace-ului, fără schimbări de algoritm.
V07 `2d5b35165de2ac6be5afa9420851fc24895c6416` a fost disponibil și încărcat
offline; DLL SHA256:
`1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A`.

## Numerică și activitate

**PASS** față de referințele native: 2.128 cazuri, 34.048 rezultate per ieșire,
eroare absolută maximă **0**, RMS al erorii **0**, diferență de fază **0 rad**,
lag de impuls **0 samples**, **0 diferențe bit-cu-bit**, **0 NaN/Inf**.
Corpusul include MUSIC și CINEMA, unity, IIR BW12, IIR + All-Pass,
FIR 2049/4097/8193, FIR 8193 + All-Pass și blocuri
1/7/31/64/127/256/511/1024 samples, pe toate cele 16 ieșiri.

Stimuli: sinus la 10/20/40/80/160/315/630/1000/2000/4000/8000/16000/
24000/40000/60000 Hz, impuls, sweep logaritmic 10 Hz–60 kHz (1 s), multitone
și zgomot LCG determinist. Sunt păstrate frecvențele de stopband pentru comparație;
cerința de semnal activ se aplică explicit în passband, unity și stimulii de bandă
largă. Harness-ul respinge deliberat un buffer mut care ar trebui să fie activ și
un buffer corupt. Pentru fiecare ieșire sunt înregistrate peak, RMS, amplitudine,
fază, primul sample semnificativ și sample-ul peak-ului de impuls.

Praguri native: max error ≤ `1e-12 + 1e-11*reference_peak`,
RMS error ≤ `1e-13 + 1e-12*reference_RMS`, semnal activ > `1e-10`.
Estimatorul sin/cos/DC este verificat separat cu amplitudine 0,25 și fază 0,3 rad.
Predicția analitică independentă folosește biquad BW12, transformata tap-urilor FIR
Blackman, All-Pass, gain și delay efectiv acceptate; praguri amplitudine
`1e-9 + 1e-6*expected_amplitude`, fază `1e-5 rad`.
Maxim observat al erorii analitice de amplitudine: **4,0631e-14**;
maxim al erorii de fază: **8,8889e-7 rad**, la un semnal de stopband ~1,23e-10.

V07: eroare absolută maximă **2,0162e-15**, RMS maxim al erorii **4,7324e-16**,
diferență maximă de fază **9,5741e-9 rad**, lag de impuls **0 samples**.
Toate cele 34.048 comparații au fost executate și sunt în toleranța numerică
nativă de mai sus. Acceptarea formală „golden V07” este **BLOCKED**, deoarece
documentația originală descrie capturi de profil și nu definește pragurile pentru
corpusul extins. Diferențele efective sunt livrate, fără a pretinde identitate
bit-cu-bit cu DLL-ul compilat separat. `native_tolerance_diagnostic` este un
diagnostic explicit, distinct de această acceptare.

**PASS**: 16 secvențe de tranziții, 512 rânduri (256 native / 256 V07),
4.096 ferestre finale de activitate/silențiere. Include gain −6/+6/0 dB,
delay, mute/unmute, emergency mute/unmute, IIR↔FIR, FIR 2049↔8193,
schimbări de coeficienți în timpul crossfade-ului, All-Pass, topologie și schimbarea
programului. Activitatea așteptată este derivată din rolurile/topologia acceptate,
independent de bufferul produs. Native: niciun eșec; V07 este diagnosticat separat.

**PASS**: 6.912 măsurători analitice gain/delay/fază/amplitudine și 768 impulsuri
de delay, toate ieșirile, ambele programe, toate blocurile, gain −6/0/+6 dB,
delay cerut 0/1/1,003 ms, frecvențe 10/1000/60000 Hz. NaN/Inf la intrare sunt
respinse fără avansarea contorului de blocuri.

Delay-ul cerut zero este acceptat de setter-ul original ca **0,00006103515625 ms**
(**0,01171875 samples**); 1 ms devine 1,00006103515625 ms, iar 1,003 ms devine
1,0030517578125 ms. Interpolarea liniară originală are răspunsul
`exp(-j*w*floor(d))*((1-frac(d))+frac(d)*exp(-j*w))`: atenuarea și faza rezultate
sunt măsurate și explicate prin acest răspuns, fără gain ascuns adăugat de graph.
Nu s-a implementat Lagrange/Thiran.

Raportarea latenței graph-ului este comparată cu cea nativă și V07 la fiecare caz.
Latența nominală FIR este 1024/2048/4096 samples (5,333/10,667/21,333 ms),
fără latență suplimentară de partiționare. CSV-ul de impuls păstrează separat
first/peak pentru fiecare canal; AP/IIR nu sunt descrise printr-o latență constantă.
Nicio latență hardware/FIFO nu a fost măsurată în acest test offline.
Sinusurile de corpus au nivel de bază 0,01, diferențiat pe surse; aceste teste
nu sunt o validare de headroom pentru ieșire fizică. Problema Bass PCA output
overrange menționată în AGENTS.md rămâne nerezolvată, iar politica Headroom
din AudioGraph rămâne fixture-ul unity offline original.

## Identitate și realtime

**PASS** Headroom în unity/bit-perfect: 0 diferențe bit-cu-bit, inclusiv `-0.0`,
testat direct și după Matrix la toate cele opt mărimi de bloc.
**FAIL** Output Matrix: semnul lui `-0.0` este pierdut prin acumularea de la `+0.0`.
Celelalte valori ale fixture-ului sunt păstrate. 8 rânduri FAIL, 2.696 sample-uri
cu semnul zero schimbat. Testul rămâne în CTest și nu a fost suprimat.
Identitatea aritmetică este PASS (eroare absolută/RMS zero); FAIL se referă
strict la identitatea bit-cu-bit, nu la o diferență de amplitudine audibilă.

**PASS** audit dinamic în aria instrumentată: 240 scenarii, minimum 200 callback-uri și 16.384 frames per scenariu
= **497.040 callback-uri**; toate contoarele new/delete, heap alloc/free,
wait, lock și filesystem I/O sunt zero. Include prima procesare, FIR 8193 + AP,
gain/delay, mute/unmute, emergency, topologie, programe, IIR/FIR și lungimi FIR,
MUSIC/CINEMA și blocuri 1/7/31/64/127/128/256/511/512/1024.
Self-test-ul hook-urilor injectează operații înainte de resetarea contoarelor.
Sursele plasează ippInit, planurile FFT, alocările workspace și warmup-ul în
construcție/pregătire, în afara callback-ului; procesarea folosește planurile existente.

**FAIL** resetări mari în callback: la primul emergency mute/topology disable,
`PluginProcessor.cpp` resetează FIR și golește `delayBuffer` și `delayEpochs`.
La 192 kHz/max block 1024, 312.328 elemente × 8 bytes × 2 × 16 canale =
**79.955.968 bytes**, plus istoricul FIR și celelalte stări. Resetarea este
condiționată de `safetySilenced`, dar produce un spike la prima adoptare.
Această matematică/sursă originală nu a fost modificată.

**BLOCKED** dovada universală pentru toate API-urile: hook-urile acoperă operatorii
C++ și importurile Windows numite din imaginile încărcate; nu certifică apeluri
rezolvate dinamic, syscall-uri directe, imagini viitoare sau API-uri neincluse.
Auditul de sursă completează măsurarea, fără a transforma această limită într-un PASS
absolut pentru întregul sistem de operare.

## CPU și benchmark

Backend-ul IPP 2026 necesită **SSE4.2**; generația 2026 a eliminat SSE3.
Sursa oficială: [Intel IPP 2026 release notes](https://www.intel.com/content/www/us/en/developer/articles/release-notes/ipp/2026.html).
Build-ul nu adaugă `/arch:AVX2` sau FMA3 obligatorii. Dispatch-ul observat pe acest
PC este `0x1007efff`. Testul cu mască SSE4.2 a trecut cele 40 comparații quick,
dar **BLOCKED** ca demonstrație fără AVX: constructorii originali apelează ippInit
și restaurează dispatch-ul complet. Nu s-a testat fizic un CPU mai vechi.

Recomandare pentru o etapă viitoare: backend nativ SSE2 selectat la pregătire,
aceeași structură FIR direct-head 128 + tail partition 128/FFT 256, aceleași tap-uri,
normalizare, ordonare temporală, delay și crossfade. Validare diferențială înainte
de activare; nu se propune acum un FIR diferit și nu s-a implementat fallback-ul.

Benchmark-ul folosește QPC, 2.000 callback-uri warmup, 10.000 măsurători steady
per rând, 200 adoptări per rând de tranziție. Refill-ul, setter-ele și serviceUpdates
sunt în afara intervalului măsurat. p99 este percentile nearest-rank; maximum
este maximum observat. Tranzițiile măsoară primul callback de adoptare după
settling-ul stării precedente; nu pretind un profil temporal complet al fade-ului.
Primul lot a folosit 200 callback-uri pentru settling; lotul continuat folosește
cel puțin 19.200 frames (100 ms), peste fade-ul de 50 ms și istoricul FIR 8193.
Numărul de adoptări măsurate rămâne 200 în ambele loturi.
Fixture-ul CPU activează toate cele 16 ieșiri pe rolul Left, cu același filtru
pe fiecare canal; cazurile FIR nu reduc sarcina prin bypass-ul LFE.
Rulează serial, fără alte procese de harness în paralel, cu scheduling Windows
normal; rezultatele includ preempții și nu sunt o garanție hard realtime.

Bugetele callback la 192 kHz: 128 = 666,667 µs, 256 = 1333,333 µs,
512 = 2666,667 µs, 1024 = 5333,333 µs. Orice maximum peste buget produce FAIL
și exit code nenul; nu este ascuns prin raportarea doar a medianei.

**Benchmark executat: 224/224 rânduri; 85 PASS / 139 FAIL.**

FIR 8193 taps, toate cele 16 canale active (µs):

| Module | Program | Frames | N | Median µs | p99 µs | Max µs | Budget µs | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | 128 | 10000 | 316.00 | 775.80 | 2333.10 | 666.67 | FAIL |
| OutputMatrix | MUSIC | 128 | 10000 | 3.90 | 4.30 | 110.20 | 666.67 | PASS |
| Headroom | MUSIC | 128 | 10000 | 3.70 | 17.80 | 111.20 | 666.67 | PASS |
| AudioGraph | MUSIC | 128 | 10000 | 330.80 | 806.20 | 1437.10 | 666.67 | FAIL |
| Crossover | MUSIC | 256 | 10000 | 585.30 | 1224.70 | 1965.30 | 1333.33 | FAIL |
| OutputMatrix | MUSIC | 256 | 10000 | 7.60 | 22.50 | 94.00 | 1333.33 | PASS |
| Headroom | MUSIC | 256 | 10000 | 7.30 | 23.50 | 162.30 | 1333.33 | PASS |
| AudioGraph | MUSIC | 256 | 10000 | 637.45 | 1397.40 | 5035.90 | 1333.33 | FAIL |
| Crossover | MUSIC | 512 | 10000 | 1370.40 | 3900.50 | 78791.00 | 2666.67 | FAIL |
| OutputMatrix | MUSIC | 512 | 10000 | 15.10 | 56.00 | 1157.30 | 2666.67 | PASS |
| Headroom | MUSIC | 512 | 10000 | 14.80 | 45.80 | 503.00 | 2666.67 | PASS |
| AudioGraph | MUSIC | 512 | 10000 | 1377.95 | 3690.40 | 6150.10 | 2666.67 | FAIL |
| Crossover | MUSIC | 1024 | 10000 | 2579.40 | 5421.30 | 19057.80 | 5333.33 | FAIL |
| OutputMatrix | MUSIC | 1024 | 10000 | 29.90 | 97.60 | 1198.30 | 5333.33 | PASS |
| Headroom | MUSIC | 1024 | 10000 | 29.10 | 107.80 | 1969.10 | 5333.33 | PASS |
| AudioGraph | MUSIC | 1024 | 10000 | 2700.20 | 5569.40 | 11262.70 | 5333.33 | FAIL |
| Crossover | CINEMA | 128 | 10000 | 422.95 | 1988.30 | 26984.30 | 666.67 | FAIL |
| OutputMatrix | CINEMA | 128 | 10000 | 3.90 | 19.70 | 130.80 | 666.67 | PASS |
| Headroom | CINEMA | 128 | 10000 | 3.80 | 21.50 | 307.90 | 666.67 | PASS |
| AudioGraph | CINEMA | 128 | 10000 | 426.40 | 2038.50 | 19764.60 | 666.67 | FAIL |
| Crossover | CINEMA | 256 | 10000 | 676.55 | 2521.00 | 4753.60 | 1333.33 | FAIL |
| OutputMatrix | CINEMA | 256 | 10000 | 7.80 | 34.60 | 1020.80 | 1333.33 | PASS |
| Headroom | CINEMA | 256 | 10000 | 7.40 | 32.90 | 1273.90 | 1333.33 | PASS |
| AudioGraph | CINEMA | 256 | 10000 | 728.75 | 2720.00 | 14980.90 | 1333.33 | FAIL |
| Crossover | CINEMA | 512 | 10000 | 1328.90 | 3616.20 | 5378.90 | 2666.67 | FAIL |
| OutputMatrix | CINEMA | 512 | 10000 | 14.80 | 52.50 | 641.00 | 2666.67 | PASS |
| Headroom | CINEMA | 512 | 10000 | 14.50 | 53.50 | 703.50 | 2666.67 | PASS |
| AudioGraph | CINEMA | 512 | 10000 | 1392.15 | 3704.90 | 5650.80 | 2666.67 | FAIL |
| Crossover | CINEMA | 1024 | 10000 | 2853.90 | 4577.70 | 6566.10 | 5333.33 | FAIL |
| OutputMatrix | CINEMA | 1024 | 10000 | 30.00 | 78.40 | 487.70 | 5333.33 | PASS |
| Headroom | CINEMA | 1024 | 10000 | 29.70 | 77.80 | 210.10 | 5333.33 | PASS |
| AudioGraph | CINEMA | 1024 | 10000 | 2950.05 | 4665.90 | 7533.10 | 5333.33 | FAIL |

Cele mai mari maxime observate (includ scheduling-ul Windows):

| Module | Program | Config | Frames | N | Median µs | p99 µs | Max µs | Budget µs | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Crossover | MUSIC | FIR8193 | 512 | 10000 | 1370.40 | 3900.50 | 78791.00 | 2666.67 | FAIL |
| AudioGraph | MUSIC | FIR8193-AP | 1024 | 10000 | 2742.95 | 5675.30 | 70595.70 | 5333.33 | FAIL |
| Crossover | MUSIC | FIR8193-AP | 1024 | 10000 | 2654.10 | 5538.60 | 47118.00 | 5333.33 | FAIL |
| AudioGraph | MUSIC | FIR8193-AP | 512 | 10000 | 1433.55 | 3875.20 | 39254.00 | 2666.67 | FAIL |
| Crossover | CINEMA | FIR8193 | 128 | 10000 | 422.95 | 1988.30 | 26984.30 | 666.67 | FAIL |
| Crossover | CINEMA | IIR-AP | 512 | 10000 | 312.80 | 699.40 | 26188.70 | 2666.67 | FAIL |
| AudioGraph | CINEMA | IIR-FIR-transition | 1024 | 200 | 3997.45 | 8090.20 | 20578.80 | 5333.33 | FAIL |
| OutputMatrix | MUSIC | IIR-AP | 1024 | 10000 | 31.20 | 113.50 | 20422.90 | 5333.33 | FAIL |
| AudioGraph | CINEMA | FIR8193 | 128 | 10000 | 426.40 | 2038.50 | 19764.60 | 666.67 | FAIL |
| Crossover | MUSIC | FIR8193 | 1024 | 10000 | 2579.40 | 5421.30 | 19057.80 | 5333.33 | FAIL |

Toate configurațiile, inclusiv FIR 8193 + AP, IIR + AP, unity și tranzițiile: [tabel complet](BENCHMARK-STAGE02.md). Valorile brute: `build/msvc-release/stage02-results/benchmark.csv`.

Controale analitice: eroare maximă amplitudine 5.55111512e-17, fază 1.86343996e-14 rad; impulsuri de delay: eroare maximă 2.83735709e-16.

V07: diferență maximă a amplitudinii sinusoidale 4.51028104e-17; native 0. Tranziții: max error native 0, V07 2.71917905e-16; RMS error maxim V07 1.65129424e-17.


## Artefacte și reproducere

Rezultate complete: `build/msvc-release/stage02-results/`:
`numerical-complete.csv` (68.096 rânduri), `parameters-complete.csv`
(17.920 valori de parametri), `transitions.csv`,
`transition-windows.csv`, `controls.csv`, `delay-impulses.csv`, `identity.csv`,
`realtime.csv`, `benchmark.csv`, `summary.json`, `final-exit-codes.json`,
`ctest.log`, `probe.log`, `controls.log`, `transitions.log`, `benchmark.log`,
`cpu-baseline.log`, `source-integrity.log`, `dsp-diff.txt`,
`stable-stage3-integrity.json` și `executable-hashes.csv`.
Build/reverificare finale: `build-final.log`, `ctest-final.log`, `probe-final.log`,
`quick-final.log`, `csv-error-regression.log`, `executable-hashes-final.csv`,
`realtime-source-audit.log` și `compile-policy.log`.
Proveniența detaliată: `artifacts/stage02/source-provenance.json`.

Corpusul numeric a fost executat în 741 cazuri seriale și apoi 1.387 cazuri în opt
procese offline independente (fiecare graph cu un singur thread); acestea s-au
încheiat înaintea benchmark-ului. Merge-ul verifică exact 2.128 chei distincte,
cu câte 32 rânduri per caz, fără duplicate sau cazuri lipsă. Fișierele brute,
`numerical-initial-complete.csv`, `shard-*/numerical.csv` și `shard-*/run.log`,
sunt păstrate. Doar schema acceptării V07 este normalizată la merge; măsurătorile
brute nu sunt modificate.

Reproducere serială: `tools/Run-Stage02.ps1`; sumarizare cu
`python tools/Summarize-Stage02.py build/msvc-release/stage02-results`.
Pentru layout-ul fragmentat al acestei execuții, sumarizarea folosește explicit
`--merge-shards --merge-benchmark`; loturile brute sunt verificate înainte de merge.
V07 absent/neverificat produce comparație BLOCKED și celelalte teste continuă.
Scriptul continuă după rezultate FAIL/BLOCKED pentru a păstra toate măsurătorile.

CTest final: **12/13 PASS**, inclusiv toate cele **10 teste originale**;
singurul FAIL este testul Matrix signed-zero. `EntropyAudioCore.exe --probe`:
**PASS**, fără hardware sau IPC. Build și --probe au fost efectiv executate.

SHA256 `EntropyAudioCore.exe`:
`320BCF01A30FA80C8E432384F9FA965DFBCB09237D37ECB0E84701407B4A8282`.
SHA256 `EntropyStage02-measured.exe` (corpus complet, controale, tranziții,
primul lot de benchmark):
`8C5BF98DE7903BCBCDC542CF8BA3E80B0784FBEC69CBDB0C073D0B82659F760D`.

SHA256 `EntropyStage02-benchmark.exe` (benchmark continuat, CTest repetat,
40 cazuri quick repetate, regresia scrierii CSV):
`714E623CB62C6488B18ECEFA81A435E34DCE2853842FF730BAB1CB48248D4C43`.
Corecțiile finale schimbă numai tratarea erorilor CSV, separarea contoarelor V07
și settling/resume în benchmark. Formulele, stimuli și comparațiile numerice sunt
aceleași. Regresia cu director de rezultate inexistent întoarce corect exit 2.


SHA256 `EntropyStage02.exe` final, cu auditul realtime extins și reverificare CTest/quick: `8317C5B07851D151A4DDA2FDD8C3376ADBFE7412FC1B7AF0EA9B88BD43208CDD`.

Ultima reverificare: `ctest-last.log`, `probe-last.log`, `quick-last.log`, `build-last.log`, `executable-hashes-last.csv`.

Execuția testelor s-a oprit după raport. Publicarea ulterioară autorizată include
numai branch-ul Stage02, fără binare, librării Intel, directoare build, merge sau
conectare fizică. Arhiva completă a rezultatelor brute rămâne local, sub
`local/stage02-archives/stage02-results-2026-10-08.zip` (ignorat de Git), SHA256
`23F7033A6FDE9A773A216F0C4CB9B5FE7F06178CD8505EB3F9D6D8BC62C30B24`.
