# Stage03 — Realtime Hardening & Intel IPP FIR Optimization

**Stage03 = PASS NUMERIC / FAIL REALTIME. Acceptarea formală V07 = BLOCKED.**
Execuția finală este completă: 15/15 CTest PASS și `EntropyAudioCore.exe --probe`
PASS. Auditul instrumentat și reseturile deterministe trec; deadline-urile CPU
nu trec în toate configurațiile. Niciun FAIL nu a fost eliminat sau toleranță relaxată.

## Origine și limite

Branch local `codex/stage03-realtime-optimization`, pornit exact din Stage02
`3b2120d980a7d6d4aa2d0b9a3d79ad3927b4dfd7`. Execuție MSVC x64 Release,
C++20, `/fp:precise`, FP64, 192 kHz, 13 intrări / 16 ieșiri.
MSVC 19.51.36257.0, Visual Studio 2026 18.10.0, Intel IPP 2026.0.0
`r0xa7ad6ebc`, CMake 3.30.5, Ninja 1.12.1, Intel Core i7-9700F.

S-au modificat exclusiv stocarea/invalidărea istoricelor Crossover și copia
Output Matrix pentru un singur contributor, plus testele, CMake și reproducerea.
Coeficienții, formulele IIR/FIR/All-Pass, normalizarea, tap-urile, crossfade-ul,
gain-ul, delay-ul și ordinea summing-ului many-to-one au rămas cele originale.
Engine, Shared, Egress, Headroom și NativeState nu au diferențe față de Stage02.
Cele patru binare observate ale Stage3 stabil au SHA256 identic cu observația
Stage02. Procesele audio deja prezente nu au fost pornite, oprite sau modificate
de această execuție. Nu s-au conectat DAC-uri, Stage2 sau Qt.

## Reset determinist și kerneluri IPP

`delayValidSamples` saturează la capacitatea ringului. Un read poate accesa numai
un index scris după reset; ceilalți indecși produc zero logic. Ambele capete ale
interpolării fracționare sunt verificate. La primul tur după reset, tag-urile de
epoch sunt scrise explicit, inclusiv zero dacă tracing-ul nu este activ.
Resetul schimbă contorul și poziția, fără ștergerea vectorilor sau transfer către
un alt thread. Nu există wrap al unui contor de generații.
Vârsta indexului folosește o scădere și cel mult o corecție de wrap, fără modulo
suplimentar; după umplerea ringului toate sloturile sunt valide până la reset.
Varianta intermediară cu modulo avea un cost IIR suplimentar. Măsurătorile ei
au fost păstrate local; corpusul și toate verificările au fost repetate pentru varianta finală.

Ringul spectral FIR are un contor saturat de partiții valide. Atât convoluția
curentă cât și reconstrucția overlap-ului anterior omit sloturile încă nescrise.
Slotul de 256 `Ipp64fc` este rescris integral înainte să devină valid. Istoricul
de epoch FIR folosește aceeași regulă pentru tap-uri. La reset rămân doar
workspace-urile fixe de head/chunk/tail/overlap, 7 KiB per canal, plus stările
IIR/All-Pass mici. Nu există clearing proporțional cu lungimea FIR sau delay.

Pentru `prepare(1024)`, un reset simultan pe 16 canale evită acum ștergerea a
85.280.768 bytes: două ringuri delay de 312.328 elemente per canal, ringurile
spectrale și istoricele epoch FIR. Workspace-ul FIR șters este fix: 114.688 bytes
pe toate cele 16 canale. Ștergerea fiecărui slot spectral în procesarea normală
este fixă și face parte din convoluția originală.

Kernelurile spectrale sunt construite direct în `Ipp64fc`, cu același FFT și
aceeași scalare. `kernelBridge` și copiile per partiție au fost eliminate.
La FIR8193 pe toate cele 16 canale aceasta elimină 4 MiB de copiere pentru
fiecare evaluare tail de 128 frames; două kerneluri în fade necesitau două astfel
de evaluări. Direct-head 128, partitioned-tail 128 / FFT256 și toate lungimile
2049/4097/8193 rămân neschimbate.

Output Matrix copiază bit-exact când există un singur contributor, inclusiv
`-0.0`; destinațiile cu zero sau mai mulți contributori păstrează inițializarea
la `+0.0`, iar summing-ul urmează aceeași ordine crescătoare a surselor.

## Verificare numerică executată

- **PASS:** 2.128 cazuri / 34.048 rânduri native / 16 ieșiri; sinus 10 Hz–60 kHz,
  impuls, sweep, multitone și zgomot determinist; blocuri 1/7/31/64/127/256/511/1024;
  MUSIC/CINEMA, IIR, FIR2049/4097/8193 și All-Pass.
- Stage02: eroare absolută maximă **0**, RMS **0**, diferență amplitudine **0**,
  diferență fază **0**, diferență latență **0 samples**, diferențe de biți **0**.
  Toate cele 68.096 rânduri native + V07 păstrează exact măsurătorile publicate
  Stage02, comparate pe cheie și 20 de câmpuri de măsurare.
- Analitic: eroare amplitudine maximă `4.0630693254328776e-14`, eroare fază maximă
  `8.888944548202105e-7 rad`; toleranțele originale au rămas active.
- **V07 diagnostic executat / acceptare formală BLOCKED:** 34.048 rânduri;
  max absolut `2.016182359954044e-15`, RMS maxim `4.73243431495252e-16`,
  fază diferențială maximă `9.574120052313617e-9 rad`, lag impuls **0**.
  Diferențe de biți V07: `895.521.176`, NaN/Inf: **0**.
  DLL verificat SHA256 `1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A`.
  Nu există prag V07 publicat pentru acceptarea formală a acestui corpus extins.
- **PASS:** 6.912 controale Gain/Delay și 768 impulsuri delay; 4.096 ferestre
  târzii de tranziție verifică explicit semnalul pe ieșirile așteptate active.
- **PASS:** 256 rânduri native de tranziție: max absolut/RMS **0**, NaN/Inf **0**;
  `1.629.976` diferențe de biți, numai semne de zero. V07: max absolut
  `2.719179048593645e-16`, RMS maxim `1.6512942362876046e-17`.
- **PASS:** 5.120 rânduri Stage02 pentru reactivare după un singur callback
  silențios: Emergency Mute, topology disable, schimbare FIR/AP în mute, mute
  cu rampă; toate configurațiile și blocurile de mai sus. Eroare absolută/RMS
  **0**, NaN/Inf **0**, `2.021.632` diferențe de biți numai pentru semne de zero.
  Istoric vechi după emergency/topology reset: **zero**. Mute cu rampă păstrează
  istoricul, conform originalului. V07: max absolut `2.1206994493816467e-16`,
  RMS maxim `1.2511620821319587e-17`; acceptare formală **BLOCKED**.
- **PASS:** reset FIR pentru fiecare lungime, zece poziții de warmup și trei
  reseturi succesive, comparat cu instanță proaspătă; epoch FIR pe 17.000 samples
  după reset, inclusiv wrap.
- **PASS:** ring delay umplut/înfășurat, delay efectiv până la 1.600 ms, istorice
  alimentate cu 400.000 samples înaintea fiecărui reset; emergency și topology,
  reactivare imediată, apoi impuls. MUSIC/CINEMA × IIR/FIR8193-AP, toate 16 ieșiri:
  max absolut **0**, diferențe biți **0**, NaN/Inf **0**, impuls activ după delay.
- **PASS:** cele 24 de rânduri de identitate Matrix/Headroom au zero diferențe de
  biți, inclusiv `-0.0`. Testul many-to-one cu anulări și valori de `1e±200`
  păstrează bit-exact ordinea originală; copia one-to-many păstrează signed zero.

Diferențele de semn ale zeroului sunt vizibile în CSV; nu sunt diferențe de gain,
fază sau latență. Oracle-ul respinge explicit ieșirile active complet mute și
datele corupte. Testul de identitate Stage02 original și cerința reprezentării
kernelului au fost executate RED înainte de implementare; logurile FAIL rămân.

Prima versiune măsurată a testului de reactivare eticheta tap-urile inițiale și
după schimbarea FIR length. Sursele au fost corectate și testul a fost repetat.
CSV-ul inițial rămâne în `initial-controls`, cu această limitare explicită.
Agregatorul poate corecta numai această metadată conform fixture-ului, păstrând
originalul și jurnalul corecțiilor; valorile măsurate și toleranțele nu sunt
rescrise. Executabilele numerice inițial și final sunt păstrate separat.

## Realtime safety și CPU

Lotul principal: **93 PASS / 131 FAIL înainte → 128 PASS / 96 FAIL după**,
câte 224 rânduri. Fade complet: **1 PASS / 79 FAIL înainte → 8 PASS / 72 FAIL după**,
câte 80 rânduri. Toate cele opt cazuri AudioGraph Emergency Mute trec după optimizare.
FIR8193 + All-Pass steady pe 16 canale rămâne FAIL la toate cele opt combinații.

AudioGraph FIR8193 + All-Pass, QPC în µs (mediană / p99 / maximum):

| Program | Frames | Stage02 înainte | Stage03 după | Verdict după |
| --- | --- | --- | --- | --- |
| MUSIC | 128 | 645,65 / 1786,4 / 3324,5 | 381,05 / 1112,4 / 3326,3 | FAIL |
| MUSIC | 256 | 954,4 / 2262,4 / 3726,7 | 785 / 1924,4 / 4162,1 | FAIL |
| MUSIC | 512 | 1708,25 / 3393,8 / 33189,1 | 1231,2 / 2506,2 / 3894,8 | FAIL |
| MUSIC | 1024 | 3073,5 / 4915,9 / 8498,2 | 2329,6 / 3913 / 14618,6 | FAIL |
| CINEMA | 128 | 839,3 / 2316,4 / 34659,2 | 381,9 / 1151,3 / 3228,7 | FAIL |
| CINEMA | 256 | 970,2 / 2338,3 / 5154,9 | 679,65 / 1652,2 / 7422,1 | FAIL |
| CINEMA | 512 | 1676,05 / 3237,8 / 5297,7 | 1242,4 / 2642 / 46383,4 | FAIL |
| CINEMA | 1024 | 3064,05 / 4808,3 / 6878,4 | 2307,85 / 3823 / 5658,8 | FAIL |

Mediana ciclurilor thread-ului pentru aceleași cazuri scade cu aproximativ
17,5–53,7%; mediana QPC scade cu 17,7–54,5%. Acestea sunt comparații în mediul
încărcat observat, nu o garanție pe un PC izolat. Emergency Mute AudioGraph
MUSIC/128: QPC `3039,15 / 9914,6 / 52485,4` → `182,7 / 399,3 / 450` µs;
mediana ciclurilor `8.899.500` → `469.050`. Eliminarea bulk reset are un efect
clar, dar tranzițiile și unele outlier-uri păstrează verdictul general FAIL.
Tabelele tuturor modulelor, programelor și dimensiunilor, inclusiv cicluri CPU
și fade complet, sunt în [BENCHMARK-STAGE03.md](BENCHMARK-STAGE03.md).

**PASS în aria instrumentată:** 240 scenarii, 497.040 callback-uri, zero C++
allocations/deallocations, zero HeapAlloc/HeapFree, locks, waits sau filesystem.
Codul nu inițializează IPP în callback: planurile, alocările și warmup-ul sunt
în constructor/control/prepare. Reseturile mari au fost eliminate prin
invalidare logică; nu s-a introdus un thread de reset. Auditul este limitat la
API-urile hook-uite și inspecția surselor, nu dovadă universală pentru API-uri
interne dinamice ale oricărei biblioteci.

Benchmark-urile rulează serial după toate procesele de validare: 224 rânduri
Stage02 înainte + 224 Stage03 după, pe același PC și aceeași instrumentare;
128/256/512/1024 frames la 192 kHz, MUSIC/CINEMA. FIR8193 activează toate cele
16 canale pe rolul Left; nu reduce sarcina prin bypass LFE. Module:
Crossover, Output Matrix, Headroom, AudioGraph. Steady: warmup 2.000,
10.000 callback-uri măsurate. Tranziții: 200 prime callback-uri de adoptare,
cu cel puțin 19.200 frames de settling al stării precedente; nu este un profil
complet al fiecărui sample din fade. Setter-ele, refill-ul, serviceUpdates și
scrierea CSV sunt în afara intervalului QPC măsurat.

Suplimentar, `EntropyStage03Fade` urmărește fiecare callback în 20 tranziții
alternante per configurație, timp de cel puțin 19.200 frames per tranziție:
filter, IIR/FIR, FIR length, FIR/All-Pass și program. 80 rânduri înainte și
80 după, Crossover și AudioGraph, ambele programe și cele patru mărimi de bloc.
Fiecare rând verifică explicit semnal activ pe toate ieșirile și lipsa NaN/Inf;
validarea este în afara intervalului măsurat. Fade-ul complet și primul callback
de adoptare sunt raportate separat.

Mediană = media perechii centrale; p99 = nearest-rank; maximum = maximum observat.
Orice callback care depășește `frames / 192000` produce FAIL al rândului, fără
relaxarea toleranței sau eliminarea outlier-ului. CSV agregat, benchmark complet
și eșantioane brute păstrează și toate rândurile FAIL.

Costul de execuție este raportat separat în **cicluri ale thread-ului**, inclusiv
user/kernel și overhead-ul wrapperului, măsurat separat cu apel gol. Perechile
brute QPC/cicluri și ciclurile callback-ului cu maximum wall sunt păstrate.
Nu convertim ciclurile în µs: documentația
[QueryThreadCycleTime](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)
interzice inferența directă, deoarece timerul CPU poate varia cu implementarea
și frecvența. Timpul QPC include scheduling și întreruperi; timpii exacti de
descheduling în µs sunt **BLOCKED**, nu deduși din diferența dintre două statistici.
`xperf` / CSwitch: **Access denied 0x5**; tokenul nu are SeSystemProfilePrivilege.
Nu s-a oprit un logger sau proces existent. Încărcarea de fundal observată în
primul lot a fost aproximativ 44–58% CPU; procesele audio preexistente au rămas
intacte. Rezultatele descriu acest mediu Windows, nu un PC izolat.

Build-ul nu introduce `/arch:AVX2` sau FMA3 obligatoriu. IPP 2026 are minimum
SSE4.2, conform [Intel release notes](https://www.intel.com/content/www/us/en/developer/articles/release-notes/ipp/2026.html).
Testul cu mască restricționată trece cele 40 comparații quick, dar dovada
fără AVX este **BLOCKED**: inițializarea existentă IPP restabilește masca
`0x1007efff`. Nu s-a testat fizic un CPU vechi. Recomandarea SSE2 rămâne backend
nativ selectat la prepare, cu aceeași structură direct-head/partitioned-tail,
coeficienți, normalizare și latență, validat diferențial înainte de activare.
Nu s-a implementat un FIR diferit, SSE2 fallback sau Lagrange/Thiran.

## Reproducere și dovezi

`tools/Run-Stage03.ps1` extrage baseline-ul din SHA-ul Stage02, verifică DSP-ul
baseline cu referințele pin-uite, compilează înainte/după folosind MSVC/IPP local,
rulează testele și benchmark-urile serial. `tools/Summarize-Stage03.py` verifică
numărul și unicitatea cazurilor și agregă numai rezultate executate.

```powershell
./tools/Run-Stage03.ps1 -IppRoot 'C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0' `
  -Python 'C:\Users\bobi\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
```

Referințele private trebuie să fie prezente în `local/ref-crossover`,
`local/ref-native`, `local/ref-v07`, conform `tools/Stage02-References.ps1`.
Exit code 1 este intenționat când există deadline FAIL sau verificări BLOCKED;
inspectați fiecare verdict și log, nu numai exit code-ul global.

Compararea finală cu CSV-ul publicat Stage02: **68.096 rânduri, 20 câmpuri
numerice, zero rânduri diferite**. Se compară după cheia caz/bloc/output/oracle/frecvență,
independent de shard. Diferența signed-zero introdusă intenționat de identitatea
Matrix apare numai în cazurile de reset/tranziție descrise mai sus.

SHA256 `EntropyAudioCore.exe` final:
`99E6F58420B4E736703A20DC469BE23942038A83A45E08F5DBE6F8B5E2E5795E`.
Executabilul harness final, folosit pentru corpus și benchmark principal:
`2184BE526E1D0C5B686831F09299CD84C1C9F63E1472F223E69961118E1D4EC3`.

Erori auxiliare păstrate: prima invocare baseline cu o cale relativă CMD a
eșuat (`'build' is not recognized`), apoi build-ul a trecut cu cale absolută.
Prima agregare a încercat să citească `status` din tabelul parameters, care nu
are acea coloană; agregatorul a fost corectat și executat cu succes. Nu s-au
rescris măsurătorile. Build-urile finale și CTest nu au erori.
Logurile RED, execuția intermediară oprită, erorile auxiliare și încercarea
CSwitch refuzată sunt păstrate local; rezultatele finale au fișiere distincte.

Rezultatele curente se află în `build/msvc-stage03/stage03-results`; binarele,
librăriile Intel, directoarele build, referințele și arhiva completă rămân locale,
ignorate de Git. Nu s-a făcut push sau merge.

Dovezile text finale sunt în `validation/stage03`, inclusiv corpusul complet,
comparațiile înainte/după, identitate, controale, reactivare, tranziții,
auditul realtime, proveniență, hash-uri și loguri. `SHA256SUMS.csv` fixează
octeții acestor fișiere. Eșantioanele brute per callback și loturile intermediare
sunt păstrate în arhiva locală, împreună cu executabilele efectiv măsurate.

Arhiva locală: `local/stage03-archives/stage03-results-2026-10-08.zip`, SHA256
`32DC8AB60C98A0718DB583A27E6DECEB5E484B62691A990E70D0E1D5EFA76FAF`.
Snapshot-ul din arhivă precede adăugarea propriului hash în acest raport;
fișierele brute și executabilele finale sunt complete. Arhiva nu este inclusă în Git.
