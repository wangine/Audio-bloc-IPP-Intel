# Exclusive — lanțul real, profilul de audiție, 2026-10-12

**Verdict:** nu s-a identificat o degradare numerică în pasajul audibil măsurat, de la ieșirea Stage1 până la bufferul PCM predat celor două DAC-uri. Utilizatorul a confirmat după restaurarea Core-ului original: „Se aude corect, dar îmi mai trebuie timp pentru mai multe melodii să verific dinamica audio live”. Nu s-a implementat o corecție audio. Calificarea privește acest semnal și acest profil; audiția pe alte melodii rămâne deschisă.

## Referințe și metoda probei

Stage1 v0003-r9, Stage2 MUSIC v0009, Stage3 0.1.9-v2-r2 64k, Music Render V2 kernel 0.3.0.11. TIDAL a rămas în loop pe semnalul original diagnostic 24/96, în modul Exclusive configurat de utilizator. Accesul nu a fost reprobat prin Initialize în această etapă. AUX nu a fost reconfigurat: configurația existentă era legacy.aux / entropy-aux:system-default.

Profilul real Stage2 este păstrat în `stage2-before.Edsp`, export copiat din `Stage2State.json` după SAVE confirmat de utilizator. Profilul real Stage3 a fost salvat prin IPC în `stage3-before.entropy`. Crossoverul, protecțiile, trimurile, întârzierile, Matrix, gain-ul Headroom −2 dB și maparea DAC-urilor au fost păstrate.

Stage2 a rămas procesul original PID 6276. Stage1 r9 a rămas procesul original. Numai Stage3 a fost înlocuit temporar cu un executabil separat care adaugă observații, apoi a fost restaurat la executabilul original și același profil. Qt Stage3 a fost închis temporar pentru conexiunea IPC exclusivă. Instrumentarea copiază în cozi SPSC prealocate; numai workerii scriu pe disc. Cititorii StagePublish sunt abonați pasivi, fără un cititor kernel suplimentar. Nu s-au schimbat kernelul, Providerul, decodorul, SRC, FIFO audio, clock, gain sau filtre.

Prima captură `observed/` a avut pierderi ale observatorului la scrierea pe disc, păstrate și raportate: Graph 1014, DAC A 891, DAC B 888. Nu este utilizată pentru calificarea completă. Captura repetată `observed2/` are **11250 înregistrări pe fiecare tap, zero observer drops, zero failed writes**. Coada de diagnostic a fost mărită înainte de pornirea grafului; bufferul audio nu a fost mărit.

## Stage1 și Stage2

| Verificare | Dovezi / rezultat |
|---|---|
| Master 24/96 → intrare Stage2 | Referința WAV originală este reconstruită offline prin biblioteca PCM/SRC existentă. Identificatoarele 997 Hz LEFT și 1237 Hz RIGHT dau același offset: 2717696 cadre la 192 kHz. |
| Comparație cu masterul reconstruit | 11520000 eșantioane/canal; eroare maximă L 4.71845e−16, R 4.16334e−16; RMS eroare L 4.20897e−17, R 3.99721e−17. Gain L 0.999999999999992, R 0.9999999999999941. Roundoff FP64; fără pierdere de nivel sau stereo. Nu este o nouă certificare RAW bit cu bit. |
| Semnal loop | Referința are 54 s; comparația folosește indice periodic pe întreaga captură de 60 s, păstrând tăcerile și tranziția de loop. Nu au fost eliminate eșantioane. |
| Stage2 L/R | **11514880 eșantioane/canal identice bit cu bit** între intrare și ieșire, după întârzierea statică de 2048 cadre / 10.6667 ms. Zero diferențe L sau R. |
| Continuitate StagePublish | 11250 blocuri Source, 11249 Dsp, 11247 blocuri comune. Zero goluri de secvență, zero schimbări MUSIC generation/revision în fereastra comună. |

EQ-ul din profil era în bypass. Center Extractor rămâne activ pe tapul brut independent. LRC alignment era activ; întârzierea L/R de 2048 cadre este verificată din PCM, nu presupusă dintr-un profil care nu serializează acea stare dinamică.

## source_generation → resetHistory()

În `Stage2/source/Host/EntropyDspEngine/Core/DspStageRuntime.cpp`, resetul local este condiționat de schimbarea oricărei source_generation sau music_session_revision. La attach există un reset separat. Nu s-a repornit Stage2 și nu s-a observat schimbare de sesiune/gol care să impună reattach în intervalele capturate.

Baseline de 60 s: **zero reseturi locale**, MUSIC generation 285, AUX 0 constant. Fereastra comună a abonaților din a doua probă: tot **zero**.

Tapul complet de la pornirea Stage3 surprinde **un singur reset local suplimentar**, determinat exhaustiv din antetele consecutive și condiția exactă din sursă; nu a fost adăugat un contor executabil în Stage2:

- secvență 970985, graphFrame 461824, QPC 639643620964;
- generații `(285,99,0) → (285,0,0)`; validity `3 → 1`; MUSIC generation 285 și revision 1 neschimbate;
- primele 451 blocuri au AUX disponibil, următoarele 10799 au AUX absent;
- generația AUX zero este sentinelul publicat când `popClocked1024()` nu livrează blocul; nu reprezintă o generație nouă autentică a streamului. Antetul singur nu certifică dacă absența provine din STOP legitim sau din lipsă temporară;
- tranziția a avut loc cu **1.84321 s înainte de audiblePresentationQpc100ns 639662073055**, cât timp DAC-urile erau încă mute în calibrare/preroll.

Stage3 DomainIngress produce atunci o singură tranziție de token, iar Crossover își schimbă epoca. **Zero reseturi în pasajul audibil continuu măsurat.** Acest mecanism de metadata trebuie păstrat ca observație, dar nu este demonstrat drept cauză a degradării auditive raportate anterior și nu a fost modificat.

## Crossover, Matrix, Headroom — același profil

Replay-ul offline folosește exact profilul salvat, aceleași 11250 blocuri de intrare capturate, tokenurile/epocile și ordinea controlului. Nu deschide DAC, pipe sau bridge kernel. Pentru fiecare dintre cele 16 canale și pentru fiecare eșantion:

| Frontieră | Eroare maximă | Eșantioane diferite |
|---|---:|---:|
| Crossover | **0** | **0** |
| Matrix | **0** | **0** |
| Headroom / Graph output | **0** | **0** |

Comparația verifică execuția/integritatea profilului existent, nu oferă o certificare independentă a tuturor algoritmilor din modulul Crossover. Nu s-a comparat o bandă filtrată cu full-range.

| Pereche fizică DAC A | Crossover / bandă configurată | RMS Graph L/R | Peak Graph L/R |
|---|---|---|---|
| CH1/2, medii joase | Crossover CH1/5, HP 80.1 Hz, LP 2040.6 Hz | 0.0446096 / 0.0448936 | 0.199621 / 0.197713 |
| CH3/4, medii | Crossover CH2/6, HP 775.6 Hz, LP 8604.7 Hz | 0.0238456 / 0.0244240 | 0.163438 / 0.163378 |
| CH5/6, tweeter | Crossover CH3/7, HP 3417.2 Hz, fără LP activ; Protector ON, SAFE MIN 3095 Hz | 0.0176500 / 0.0178259 | 0.165576 / 0.165569 |

Restul DAC A: CH7 center, CH8 sub. DAC B CH1 primește scoop / Graph CH9; celelalte canale DAC B sunt nealocate și primesc numai cuantizarea/ditherul existent. Matrix: crossover CH12 → Graph CH9. Spectrul și stereo din fiecare bandă sunt implicit identice referinței replay deoarece toate eșantioanele coincideau exact; RMS/peak complete sunt în `buffer-measurements.json`.

## ClockSync și PCM predat DAC-urilor

Fiecare DAC este 8ch / 192 kHz / PCM24 left-aligned în container 32 bit. FIFO 65536, target ClockSync 4096, buffer hardware 1024. Nu s-a schimbat niciuna dintre aceste valori.

| Măsurare | DAC A | DAC B |
|---|---:|---:|
| Înregistrări sink | 11250 | 11250 |
| Goluri deviceFrame | 0 | 0 |
| ClockSync resync / reset reason | 0 / 0 | 0 / 0 |
| Blocuri respinse / incomplete | 0 | 0 |
| Deadline misses | 0 | 0 |
| Callback maxim, inclusiv copia audit | 2353 µs | 1477.1 µs |
| Termen per buffer | 5333.33 µs | 5333.33 µs |
| FIFO disponibil minim în redarea audibilă | 2033 | 1794 |
| Clock fill minim | 3071.9831 | 3071.9868 |
| Blocuri mute interioare | 0 | 0 |
| Overrange / clipping în semnalul capturat | 0 | 0 |

Preroll/calibrare: 776/775 blocuri sink mute inițiale. Primul buffer audibil are prefix PCM mute observat de 160/716 cadre. Mecanismul existent permite un asemenea prefix până la common audible onset, dar durata exact intenționată nu este verificată independent: timestampurile presentation/onset ale callbackului nu sunt în înregistrarea sink. Copia FP64 este înaintea acestui prefix PCM; cele 160/716 cadre sunt păstrate și exceptate explicit de la comparația FP64/TPDF, nu eliminate din captură. Erorile neajustate sunt păstrate în `buffer-measurements.json`. După pornire nu există blocuri mute interioare. Zerourile individuale admise de cuantizare/dither nu reprezintă goluri de PCM.

Verificare offline ClockSync pe grila fiecărui al optulea eșantion, toate canalele, folosind poziția Graph, ppm capturat și kernelul sinc existent: **10711056 eșantioane DAC A, 10711888 DAC B**. Eroare maximă A 3.07442e−9, B 1.60014e−11; zero erori peste 1e−8. Este verificare numerică eșantionată, nu identitate bit cu bit. Cele 1790/1814 poziții de la marginea capturii fără vecinătatea sinc completă sunt raportate separat; nu sunt cadre audio pierdute.

PCM final: cele **10725376 / 10726400 cadre pe fiecare canal** din bufferele declarate audibile sunt păstrate. Cu excepția prefixelor startup de mai sus, fiecare eșantion este comparat cu FP64 plus cuantizarea TPDF deja existentă: zero eșantioane în afara limitei ±1.5 LSB PCM24. Eroare maximă A 1.49604 LSB, B 1.49358 LSB. Nu s-a adăugat dither sau compensare. Bufferul observat este chiar bufferul transmis `sink->submit()`, iar accepted_frames este verificat.

## Limită importantă: două DAC-uri

`softwareAlignmentVerified=false`, `analogSkewMeasured=false`. Telemetria `estimatedABDifferenceFrames` variază de la −59.071 la +283.891 cadre și ajunge la +118.949 cadre la final. Sunt estimări de aliniere din clock/presentation, **nu măsurări analogice**. Nu certificăm sincronismul fizic A/B și nu atribuim acestei estimări lipsa înaltelor sau stereofoniei: ambele perechi L/R de tweeter și medii joase sunt pe același DAC A. Nicio corecție ClockSync sau de delay nu este justificată doar de această estimare în proba care acum se aude corect.

## Decizie și referință locală

Prima frontieră cu degradare în pasajul audibil: **niciuna demonstrată până la predarea PCM către DAC**. Nu s-au schimbat algoritmii audio. Au fost adăugate numai tapuri opt-in și două unelte offline independente de redare. Kernel 0.3.0.11, Provider RAW, Stage1 r9 și executabilele de producție Stage2/3 sunt păstrate ca referințe.

Capturile complete rămân local în acest director; `observed2/` este captura calificată. Git primește sursele, uneltele, rezultatele, profilurile, manifestele/hashurile și runtime-urile complete, fără cele aproximativ 20 GB de capturi diagnostice. Fișierele binare mari sunt păstrate local, nu șterse. Verificarea pe alte melodii nu este declarată finalizată.

Fișiere de dovezi: `baseline-header-analysis.json`, `stage2-comparison.json`, `graph-generation-transitions.json`, `known-source24-comparison.json`, `offline-replay.json`, `dac-reference.json`, `pcm-verification.json`, `buffer-measurements.json`, `observation2-telemetry.jsonl`, `observed2/*-summary.json`.

## Verificare finală și proveniența uneltelor

Core-ul original restaurat, PID 19780, raportează RUNNING, armed=true, physicalArmed=true, liveFault=false, inputMuted=false, zero scheduler faults și zero callbackWallDeadlineMisses în ultima citire (`final-restored-live-status.json`). Interfața Qt a fost redeschisă separat, fără restart Core sau schimbare de profil.

Cititorii offline finali au fost rulați pe captura completă și păstrează rezultatele de mai sus. Nouă verificări țintite ale uneltelor au respins capturi lipsă, trunchiate sau nonfinite și au verificat prin mock restaurarea Core-ului în trei cazuri de eșec. Nu s-au rulat suite generale.

Arhiva de diagnostic conține Core-ul de observație efectiv testat live. Sursele finale includ în plus verificarea flush/close și refuzul reutilizării unui director de captură nevid; această întărire a fost compilată, dar nu calificată printr-o nouă probă live. Executabilul Stage3 de producție nu a fost înlocuit. `reference-manifest.json` distinge aceste binare, iar `capture-sha256.json` păstrează identitatea fișierelor PCM locale. Arhivele runtime au fost verificate CRC, iar binarele Stage2/3 din arhive coincid cu referințele locale.
