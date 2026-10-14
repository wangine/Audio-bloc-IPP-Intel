# Entropy Stage A — prima frontieră PCM (Tidal 24/96)

14 octombrie 2026. Investigație limitată la proveniența PCM înainte de renderer. Tidal rula deja pista `Entropy_TIDAL_Diagnostic_24bit_96000Hz` în loop; utilizatorul a confirmat modul Exclusive. Nicio comandă de playback, ARM/DISARM, restart sau modificare de profil/driver nu a fost trimisă. Testul senderului local determinist rămâne separat.

## CE A FOST MĂSURAT

### Referința și identitățile

- FLAC 24/96 din Downloads: SHA256 `5d3bd26bda03b6b9617c50a17fc3069fc9bc09a36457acd7ef57f4ae227e341c`. WAV-ul extras din arhiva furnizată: SHA256 `505d7166231da2d7ab204aeeb5c53cf900893eb51dce7ed1da93e7f824768468`. PCM RAW packed 24-bit interleaved L/R din WAV: SHA256 `9f6a8741f11cf692f124d673f05ae07a13f1814da0282c58b37c0fb859d68f1e`, 5.184.000 cadre la 96 kHz, 54 s. Aceste hashuri identifică fișierul local, nu demonstrează octeții livrați de Tidal.
- Checkout Stage3 `chatgpt/stage3-audio-recovery-20261013`, HEAD `a016495d9dc1cffa3cb3e1e46063f57801727667`, status curat înainte și după. Core Stage1 r7 SHA256 `21feffbc6e3f4caaf2ab58cb1fb5c272641c04672494420d4bbd9c13dfce3a7e`; provider RAW `3329d657956a42a6e83c28850ee457d1a3a9bf9ac327955825621732b6c4b0de`; Stage2 v0010 `861c1b2872c757810e0b7af891eaa54d0597320b15441a4c904bd3fa8a237efe`; Stage3 R3 `e915d0080baf555e35f209bba68133a70aebaa0b47c0f726cd4c1a4ab0f16c73`. PID-urile producătorilor din sesiunile capturate: 6376 și 19200.
- `faust` nu a fost găsit în PATH; nu a fost instalat ori căutat pe disc.

### Sesiunea reală și capturile

Fereastra Stage1 indica MUSIC pe `Entropy Music Render V2 - raw PCM`, **24/96 active**, `Raw PCM`, dar „WASAPI access unverified”; indicatorul nu dezvăluie containerul și biții valizi efectiv negociați. AUX indica `Entropy AUX Capture`, **24/44,1 active**, Exclusive, peak curent `−inf dBFS`. În fișierul de configurare, MUSIC are `mode=8`, AUX `mode=2`; acești coduri nu au fost reinterpretate ca format WASAPI. Headerul publicației Stage1 avea `validity_mask=3` pentru toate blocurile: ambele intrări erau marcate valide. Codul `Engine/Matrix/InputMatrix.cpp` însumează M+A pentru acest mask.

Două instanțe ale observatorului existent `PassiveCapture.exe` (SHA256 `62c5296ca6d144fbe23383f420a6d61f4c46130891607bbb7c66ecab3e0e51d7`) au abonamente StagePublish V2 **independente**, cu cozi proprii, fără a consuma coada Stage3. Copierea PCM se face în memorie prealocată; fișierele se scriu după observație. Prima conectare în sandbox a returnat Windows 5; conectarea cu tokenul normal Windows a reușit. Durata cerută a fost 112 s; ambele abonamente au terminat mai devreme cu `ReadResult::Gap` (`terminal=2`). Nu au existat discontinuități în interiorul blocurilor salvate.

| Captură | ABI/layout | Cadre și interval | Secvență | Metadate |
|---|---|---:|---:|---|
| Stage1 FP64 | `0x00010000`, stereo2, planar binary64 LE, 192 kHz, 1024 cadre/bloc | 16.927 blocuri; 17.333.248 cadre; 90,277 s | 12.228.188–12.245.114 | sesiune `8c3f090d82fb3d44bea94310bbeca243`, epoch 1, source generation MUSIC 217, revision 4, source rate 96 kHz, validity 3; 0 salturi interne |
| Stage2 publicat | aceeași sesiune, L/R/CX 3 canale, FP64/192 | 17.109 blocuri; 17.519.616 cadre; 91,248 s | 12.228.006–12.245.114 | epoch 1, source generation 217, revision 4, source rate 96 kHz, validity 3; 0 salturi interne |

Primul `first_frame` Stage1 este 12.521.663.488; ultimul 12.538.995.712. Stage2 începe cu 182 blocuri mai devreme și se termină la același bloc. Timestampurile `qpc_timestamp`/`receive_qpc` sunt tickuri QPC brute la frecvența declarată de **10.000.000 Hz**, nu timp de eșantionare și nu un câmp QPC transformat în 100 ns. Ambele capturi se încheie după secvența 12.245.114; `Gap` al observatorului nu dovedește singur un gap în publisher sau în renderer.

### Comparațiile PCM

Fereastra Stage2 arăta EQ și Dynamic Recovery **`BYPASS / EXACT WIRE`**. Center V2 și LRC erau active. Dialogul LRC indica o întârziere aplicată **2662 cadre** (13,865 ms) pentru L și R și 0 pentru CX. După această întârziere fixă, fără realiniere per bloc, **17.330.176 mostre/canal** din Stage2 L/R coincid bit cu bit cu Stage1. Diferențe: 0 pe L și R; eroare maximă și RMS: 0. Cele 185 blocuri Stage2 necomparabile sunt margini ale intervalelor capturate, inclusiv cele 182 blocuri anterioare începutului Stage1 și istoricul necesar întârzierii. Centrul CX este procesat și nu a fost inclus în verdictul de bypass L/R.

Am construit o referință offline cu `MultiChannelSrcEngine.cpp` din sursa Stage1 r7 verificată (SHA256 `7faae157b71bd23d8d1012a7962126662d000aee673ef3dcf52c14161fce7c0a`), r8brain-free-src **7.3**, fază liniară, tranziție 2%, atenuare 217 dB, FFT Ooura FP64 implicit (`R8B_IPP=0`, `R8B_PFFFT=0`, `R8B_FLOATFFT=0`). PCM24 a fost convertit exact la FP64 ca în decoderul Stage1 (`int24 / 8388608.0`). După 5.184.000 cadre de intrare, am adăugat explicit **2048 cadre de zero** la intrarea SRC pentru coadă și am păstrat exact **10.368.000 cadre FP64/192** (54 s). Nu am ajustat gain-ul și nu am resamplat capturile pentru aliniere. Alinierea unică față de captura Stage1 este offsetul **444.416 cadre**, ales cu ancore bit cu bit și secțiunea stereo densă; această secțiune are 0 diferențe de biți la offsetul ales. Ferestrele markerelor de sincronizare L la 0,25 s și R la 0,95 s au, separat, 0 diferențe numerice.

Pe toate cele **10.368.000 cadre/canal**, Stage1 și referința offline sunt **numeric identice**: eroare maximă 0, RMS 0, gain estimat 1,0. Sunt doar diferențe de reprezentare a zeroului: L **1.139.946** și R **1.139.948** mostre `−0.0` versus `+0.0`; toate celelalte patternuri FP64 coincid. Aceasta este o comparație de integrare prin același cod r8brain, nu un verdict independent despre calitatea filtrului.

Comenzi de achiziție și analiză (toate ieșirile sunt în acest director):

```powershell
& 'D:\Projects\3Stage-3\Diagnostics\Exclusive-RealChain-20261012\tools\PassiveCapture.exe' '\\.\pipe\Entropy.StagePublish.Music.Source.V2.S-1-5-21-712298930-2912964222-1649560554-1001' '.\stage1-fp64.bin' 112
& 'D:\Projects\3Stage-3\Diagnostics\Exclusive-RealChain-20261012\tools\PassiveCapture.exe' '\\.\pipe\Entropy.StagePublish.Music.Dsp.V2.S-1-5-21-712298930-2912964222-1649560554-1001' '.\stage2-fp64.bin' 112
& '.\offline_ref\build.cmd'
& '.\offline_ref\build\offline_ref.exe' '.\reference-24-96.wav' '.\offline-r8brain-fp64.bin'
& 'C:\Users\bobi\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' '.\analyze_pcm.py'
& 'C:\Users\bobi\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' '.\compare_reference.py'
```

## PRIMA DIFERENȚĂ SAU FRONTIERĂ ÎNCĂ NEVERIFICATĂ

Prima diferență **de biți** față de referința SRC este la cadrul de referință **1**, canalul **L**: `0x8000000000000000` (`−0.0`) în referință și `0x0000000000000000` (`+0.0`) în Stage1. În captura live: secvența **12.228.622**, cadru absolut Stage1 **12.522.107.905**, epoch 1, generation 217, revision 4. Fereastra exactă FP64 și timestampurile QPC sunt în `first-bit-difference-window.json`. Diferența este numeric zero și nu susține o cauză audibilă. Având `validity_mask=3`, însumarea M+A din Stage1 este explicația cea mai directă pentru normalizarea semnului lui zero; aceasta este **inferență din cod și PCM**, deoarece AUX separat nu a fost capturat.

**Nu s-a găsit o diferență numerică** pe L/R până la publicația Stage2 în fereastra completă de 54 s. Prima frontieră strictă încă neverificată este **RAW înainte de SRC**: providerul Stage1 creează `AuditObserver` doar dacă procesul este lansat cu `ENTROPY_MUSIC_AUDIT_PIPE` (`.../provider/AuditObserver.cpp`); sesiunea live nu a furnizat această captură. Astfel, containerul/valid bits efective și identitatea octet cu octet a livrării Tidal nu sunt demonstrate, deși ieșirea Stage1 este numeric identică cu referința locală după SRC. Următoarea frontieră de interes pentru defectul audibil este **ce a consumat exact Stage3 înainte de crossover**: abonamentul nostru la publicația Stage2 nu este o captură din interiorul `Stage2Ingress::tryRead` al Core-ului R3. Codul `Engine/Diagnostics/ChainAudit.h` are tap pentru intrarea grafului, dar îl pregătește numai la pornire cu `ENTROPY_CHAIN_AUDIT_DIR`; Core-ul live nu a fost repornit. În ruta `stage2_`, headerul sursă nu este populat în `chain_audit::context` din `AudioScheduler.cpp`, deci un diagnostic ulterior trebuie să păstreze explicit și headerul Stage2.

## URMĂTOAREA INTERVENȚIE MINIMĂ SUSȚINUTĂ DE DATE

La o repornire **controlată de utilizator** a Stage3, pregătiți un director nou gol pentru `ENTROPY_CHAIN_AUDIT_DIR` și un build diagnostic izolat care copiază `StageBusBlockHeader` al `stage2_->lastBlock()` în tapul grafului. Comparați `graph.input` pre-crossover cu publicația Stage2 la aceeași sesiune/epoch/secvență, pe un singur pasaj Tidal 24/96. Aceasta testează exact următoarea graniță care poate încă explica PCM greșit înainte de renderer; nu cere schimbarea DSP-ului sau a DAC-urilor.

Pentru certificarea strictă a sursei, pregătiți separat un sender local determinist și o sesiune Stage1 izolată cu hookul RAW `ENTROPY_MUSIC_AUDIT_PIPE`, o singură sursă MUSIC sau AUX activă, formatul endpointului declarat și octeții trimiși salvați. Nu porniți acest test peste Tidal Exclusive live și nu modificați configurația live pentru a forța identitatea. Etapele B/C, sinkul nou și ASRC rămân în afara acestei investigații.

Artefactele și SHA256-urile sunt în `manifest.json`; capturile PCM brute și scripturile sunt păstrate în acest director. Nu au fost încărcate fișiere audio sau telemetrie în GitHub.
