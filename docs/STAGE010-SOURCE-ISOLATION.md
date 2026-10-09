# Stage010: izolarea Core-ului de lifecycle-ul și pacing-ul sursei

## Rezultat

**PASS pentru etapa 01 din audit.** Core poate opri/reporni/închide forțat abonamentul fără eliberarea sau faultarea sursei. Pereche: AudioCore **0.1.0**, Stage2 **v0007-source-isolation**. Branch separat `codex/stage010-source-isolation`, bază Core `5b3c4ba59a7a41a950658d86fdf5692e46d02fa4`. Fără push/merge.

## Intervenții

- Stage2 are permanent un singur driver local `DspLocalDemand`; busul V1 intern rămâne deținut local. V1 Attach/Release extern este refuzat și nu apelează `DspAutoLink::finish()`.
- StagePublish V2 copiază exact cele 11 plane deja procesate după commit-ul V1. Nu dublează DSP și nu schimbă coeficienți, gain, rate sau ordinea canalelor.
- Fiecare dintre maximum patru abonamente are coadă SPSC prealocată cu 16 sloturi. Full afectează numai livrarea abonamentului; producătorul nu modifică read-index și nu suprascrie sloturi active.
- Core nu mai are StageLink Attach/Release/demand sau `IngressCadence`. FIFO/DAC recovery nu controlează ritmul upstream. Conectarea, heartbeat și unmap se fac în workerul de conexiune, nu în `tryRead()`.
- Drop, salt de secvență/frame/epoch sau header invalid produc fault local, fără audio vechi redat după reabonare. Abonamentul nou începe la cursorul real al sursei. NaN/Inf rămân respinse local.
- Comenzile acceptate Matrix/Headroom se aplică la limite logice și fără input, fără procesare de mostre fabricate. Comenzile Crossover care necesită audio rămân neaplicate până la primul bloc real. Resetul quiescent recompilează tag-ul ultimei comenzi audibile, chiar dacă ulterior a sosit un gesture; nu inventează confirmări audibile.
- UUID, epoch, first_frame, block_sequence, QPC, music_session_revision, source_generation și mask-urile originale rămân reale. API-ul nativ existent primește epoch-urile originale; nu se inventează generații la fiecare flag.

## Distincția verificată pe sursa reală

Prima încercare end-to-end a eșuat înainte de primul bloc Core: transportul trata orice `discontinuity_mask` ca gap de livrare. Dovezile FAIL sunt păstrate în `validation/stage010/live-source-flags-red.json`.

Citirea exclusiv read-only a mappingului Stage1 a arătat secvență continuă, zero drop V2 și mask=1 repetat. Implementarea originală Stage1 OR-ează `telemetry.discontinuity_flags`, care include și indicatori persistenți despre modul/rata endpointului. Un astfel de flag nu demonstrează un bloc lipsă. V2 îl transmite bit-exact; Core expune `sourceDiscontinuityMask` și `sourceFlaggedBlocks` (număr de blocuri marcate, nu număr de evenimente presupuse). Gaps de coadă/seq/frame/epoch rămân terminale. Regresia cu flag repetat a avut RED înainte de corecție și GREEN după; niciun flag nu este șters.

## Dovezi end-to-end

UUID Stage1 și Stage2: `f2c7171dd4ce434580d7982b57892db6`, identic în toate cele opt observații. Mappingul Stage1 a fost deschis **FILE_MAP_READ**: testul nu a trimis comenzi și nu a avansat cursoare. Stage1/2 nu au fost repornite sau oprite pentru cleanup în scenariu. DAC-urile au rămas închise.

| Observație | Stage1 blocuri publicate | Stage2 blocuri procesate | Core blocuri |
|---|---:|---:|---:|
| Fără Core, început | 94394 | 94355 | — |
| Fără Core, după 2 s | 94769 | 94737 | — |
| Prima pornire DISARMED | 95001 | 94963 | 204 |
| Core oprit normal, absent 2 s | 95387 | 95383 | — |
| Core repornit | 95608 | 95574 | 203 |
| Core închis forțat, absent 2 s | 95989 | 95954 | — |
| Reabonare după kill | 96208 | 96184 | 203 |
| Final: Stage1/2 continuă | 96406 | 96375 | — |

Zero fault Stage1/2 în scenariu; zero gap în cele trei Core-uri conectate. Music meters nenule. Drop-urile abonamentului omorât nu sunt declarate zero; raportul brut păstrează contorul Stage2 `publish_drops`. Snapshotul Stage2 are cadență 200 ms, deci valorile nu sunt sincronizate exact cu citirea mappingului Stage1.

## Verificări

| Verificare | Rezultat |
|---|---|
| Core: 29 verificări funcționale, ABI, module, ingress, profile, Thiran unit, ARM autorizare și probe | 29/29 PASS |
| Transport V2 în CMake Core | 1/1 PASS |
| Stage2: lifecycle, publicare, DSP routes originale și audit steady-state | 12/12 PASS |
| Transport cross-process MSVC ↔ MinGW | 1/1 PASS în fiecare direcție |
| Scenariu real Core stop/restart/forced kill | PASS |
| Core Qt backend și surse DSP/Egress/AudioGraph față de baza 0.0.9 | Nicio modificare |
| Cinci DLL-uri Stage2 față de v0006 | SHA256 identic |

Core: MSVC **19.51.36257 x64**, Intel IPP **2026.0** static, fără AVX2 obligatoriu. Stage2: MinGW **13.1**, Qt **6.11.2**. Cele 138/151 fișiere Stage2 originale neschimbate și hash-urile surselor DSP sunt în `stage2-integrity.json`; modificările rămase sunt integrare, diagnostics, tests și versiune.

Auditul `tryRead()` include zero new/delete, heap alloc/free, wait, lock și I/O pe traseele testate, inclusiv gap/invalid. Stage2 a verificat 1000 cicluri externe și 1000 locale, cu abonament V2 Full, cu aceleași contoare zero în procesare. Acoperirea este cea documentată de instrumentarea importurilor; nu este o demonstrație a tuturor syscall-urilor posibile. Service-ul abonamentelor face alocare, locking și verificări de proces în control, în afara pasului DSP; nu declarăm întreg workerul Stage2 fără locks.

Testul inițial V1 reproducea eliberarea upstream și imposibilitatea de restart cu aceeași sesiune (`stage2-lifecycle-red.txt`). Testul controlului fără input reproducea starea blocată (`control-starvation-red.txt`). Ambele defecte au teste de regresie PASS.

Nu s-au repetat benchmarkurile CPU/validările lungi istorice: Stage04 Offline, ClockDrift, SchedulerProbe, Stage05Validation, ThiranValidation și V07Integration. **FAIL REALTIME Stage04** și **V07 formal BLOCKED** nu sunt modificate sau prezentate drept PASS.

## Reproducere

Build Core: `tools/Build-Windows.ps1 -Stage04 -Stage05 -BuildFolder build/msvc-thiran -CompileOnly`, cu `-Stage04Reference` și `-StableStage3Directory` către referințele locale documentate.

Build/test Stage2: `Integration/Stage2/tools/Build-Stage2V0007.ps1`; package separat: `Integration/Stage2/tools/Package-Stage2V0007.ps1`. Transport fără IPP: CMake standalone în `Tests/StagePublishV2`.

Core CTest funcțional: `ctest --test-dir build/msvc-thiran -E "EntropyStage04(Offline|ClockDrift|SchedulerProbe)|EntropyStage05Validation|EntropyThiranValidation|EntropyV07Integration" --output-on-failure`.

Scenariul live se lansează explicit prin `Tests/Stage010LiveIsolation.py --core <exe> --profile <profil> --source-discovery <Stage1/state/StageBusSession.json> --stage2-diagnostics <diagnostics.json> --output <dosar>`. Acesta oprește numai Core-urile proprii și nu autorizează DAC-uri. Stage2 publică diagnostics cu `--headless --automatic --diagnostic-file <cale>`.

## Limite și integritate

Referința executabilă Stage2 v0006 este `82CA4821DF5651BED38D9841F3BD1CA8794574B0F75EE1E03439FAE2012FE1BE`, identică instalării, runtime-ului și buildului candidate-stage2-eq. Nu s-a atribuit un commit original neverificat binarului Stage2. Noua sursă se află izolată în `Integration/Stage2/source`.

Core nou SHA256: `1F17AA286CE556E94271FFDAE74C8F762340670C7BA639F9BEC80812B150DA7C`.
Stage2 nou SHA256: `117B020DDF84587D266BFE71A58862B04D423A7111EF5B66AD9E5C88428D5F8A`.
Qt neschimbat SHA256: `431404FDDD05D6FE174944AA11FDB32EFB6E8ABAEEEC4209049DA4A973381074`.

Stage2 poate încă afecta Stage1 la propria oprire. Modul manual legacy `--source-session` este păstrat pentru fixture-urile vechi și nu are garanția live V2. Nu s-au modificat Stage1, Stage3 stabil sau algoritmi DSP. Ascultarea și validarea completă a profilelor live rămân testul utilizatorului.
