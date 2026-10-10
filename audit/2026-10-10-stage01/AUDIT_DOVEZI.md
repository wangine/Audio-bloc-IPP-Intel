# Audit 0.1.5 — dovezi și limite

## 1. Identitatea versiunii

Tag `v0.1.5` → commit `3afb8d77b3bdd984a2f5bb53095cb99c37de72c0`.
Release-ul declară implementarea comutării MUSIC/CINEMA și păstrează explicit limitările istorice de calificare și overrange. Nu confundăm numele directoarelor `stage015`/`stage016` cu numerele produsului: `stage015/numeric.json` descrie candidatul 0.1.4; `stage016/live-switch.json` conține comutările 0.1.5.

## 2. Independență — îmbunătățire confirmată în surse

`Integration/Stage1/.../SourceController.cpp::audioWorker` produce la propria cadență și apelează `publisher_.publish()` fără să aștepte o cerere de frame de la Stage2/3. `DspStageRuntime::step` consumă Stage1 și publică Stage2 fără a condiționa procesarea de existența Stage3. `Stage2Ingress` este acum abonat pasiv V2.

`StagePublishV2::Publisher::publish` are cozi per abonament. O coadă plină incrementează drops și nu oprește producătorul. Subscriber-ul identifică pierderea, retrage local abonamentul și nu renumerotează golurile. Acest mecanism este diferit de vechea cascadă de fault V1. Validarea de continuitate rămâne importantă și nu trebuie eliminată pentru a cosmetiza contoarele.

## 3. Comenzi lente — mecanism confirmat, nu doar presupus

`CoreRuntime.cpp` apelează `LiveProfilePreflightCache::run()` sincron înainte de ARM și înainte de comutări live. `LiveEligibility.cpp::liveProfilePreflight` creează o copie a grafului, o pregătește și execută DSP sintetic pe un thread înregistrat MMCSS Pro Audio / CRITICAL.

ARM fără cache execută cel puțin 940 warmup + 1000 blocuri cronometrate. Comutarea cu scenă sursă execută 940 + 20 de tranziție + 940 + 1000. Acestea sunt 1940, respectiv 2900 de blocuri a câte 1024 mostre. Duratele de 10,35/15,47 secunde sunt AUDIO SIMULAT, nu timp de așteptare real; costurile de ordinul sutelor de microsecunde/milisecunde pe bloc explică secunde de calcul.

Dovada publicată `stage016/live-switch.json` arată cache miss la ARM și la toate cele trei comutări, 1000 iterații și mediane de aproximativ 0,63–0,96 ms în blocurile cronometrate. Nu conține măsurarea completă click→răspuns, deci auditul nu pretinde o durată exactă a apăsării.

`CommandServer::worker` execută handler-ele serial. Nu poate admite un DISARM nou cât timp execută benchmark-ul anterior. `IpcClient::send` prelungește la 30 s bugetul de răspuns pentru preflight și comenzile aflate în urma lui. Aceasta evită un timeout, nu rezolvă latența comenzii.

Separat, `LiveSinkWorkers::stop` face join pe thread-urile DAC. Așteptarea WASAPI nu are eveniment de anulare în `waitBuffer`, iar apelurile driverului pot avea durată proprie. O așteptare nominală de eveniment de 100 ms nu dovedește singură un DISARM de 2–3 s. Trebuie separate: coadă IPC, gate audio, Stop/Close driver, confirmare UI.

## 4. ARM resetează inutil graful viu

Pe traseul ARM, `CoreRuntime` oprește/repornește `AudioScheduler`. La repornire, `AudioScheduler::start` cheamă `crossover().resetQuiescent()` dacă au mai fost blocuri și `stage2_->restartSubscription()`. Se pierd istoricele Crossover și se reface abonamentul doar fiindcă se deschid DAC-urile. Astfel re-ARM nu este o operație exclusiv de egress.

Corecția nu este eliminarea resetării atunci când există un gap real; este păstrarea grafului/abonamentului continuu pentru operații strict ARM/DISARM, cu proprietate sigură a resurselor DAC și invalidare locală explicită atunci când există o discontinuitate reală.

## 5. FFT și răspunsul desenat — trei defecte distincte

### 5.1 Grile de frecvență incompatibile

`CrossoverModule.cpp::getViewTelemetry` calculează 256 puncte ale răspunsului pe 20…60000 Hz, pentru toate ieșirile. `CrossoverPage.cpp` atribuie însă acelorași indici o grilă 20…5000 Hz la Bass și 20…40000 Hz la LR în modul 40k și la Center. Nu resamplează curba; o reetichetează.

Formula erorii de afișare este:

`f_afisat = 20 * (Fmax_UI / 20) ^ [log(f_real / 20) / log(60000 / 20)]`.

Exemple: un punct real de 119,5 Hz apare la aproximativ 68,615 Hz în panoul Bass; 1000 Hz apare la 820,275 Hz într-un panou cu maxim 40k. Spectrul are propriile valori de frecvență, deci curba și spectrul pot să nu mai coincidă pe ecran. Aceasta NU dovedește că filtrul audio taie la frecvența greșită.

### 5.2 Dublă compensare de amplitudine

Fereastra Hann este normalizată deja cu `N/windowSum`. FFT forward nu este împărțit la N (`IPP_FFT_DIV_INV_BY_N` normalizează inversa). Codul aplică apoi `4/N` în loc de `2/N` pentru binurile interioare ale spectrului unilateral. Rezultatul este +6,0206 dB pentru un sinus centrat în bin.

Reproducerea separată din `CALCULE_NUMERICE.json`: sinus 1500 Hz, amplitudine 0,125, N=16384. Corect: −18,0618 dBFS peak. Expresia actuală: −12,0412 dBFS peak. Corecția este a afișajului, nu a PCM24.

### 5.3 Decay după numărul de poll-uri, nu după timp

`smooth = max(db, smooth - 1.5)` scade 1,5 dB la fiecare actualizare. UI cere telemetrie la 100 ms; în aceste condiții o cădere de 60 dB durează aproximativ 4 s. Dacă serverul este ocupat cu preflight, decay-ul se prelungește. Ferestrele vechi pot rămâne disponibile fără o vârstă explicită.

N=16384 la 192k are fereastră 85,333 ms și bin de 11,71875 Hz. Nu este o fereastră audio de câteva secunde. Corecția: decay în dB/sec calculat din timestamp-ul cadrului, identitate canal/scenă/generație, fără redistribuirea unei ferestre vechi sub eticheta nouă.

În plus, răspunsul IIR desenat este un model analogic din parametri, iar FIR este aproximat cu o funcție sigmoidă. Nu reprezintă exact coeficienții/kernelul aplicat și limitele efective ale canalului. Calculul trebuie scos din citirile obișnuite ale controlului și făcut din configurația aplicată, în afara callback-ului.

## 6. Bufferele și DAC-urile

SPSC-ul copiază double, cu publicare acquire/release; pe traseul normal citit nu face mediere, compresie ori filtrare a mediilor. 1024 mostre înseamnă 5,333 ms, target 4096 înseamnă 21,333 ms. Capacitatea 32768 este 170,667 ms de stocare, NU o latență fixă obligatorie.

Rămâne o problemă de cronologie: fiecare DAC face independent open/preroll/start, iar `EgressClockEngine::render` inițializează `readPosition = writeFrame - target`. `feedUntilTarget` poate transfera până la 4096 de mostre, depășind necesarul. Payload-ul FIFO nu păstrează asocierea graph frame ↔ device frame ↔ moment QPC de prezentare.

Prin urmare, două stări LOCKED nu demonstrează aliniere A/B. Frecvența ceasului și originea temporală sunt probleme diferite. DIFERENȚA DINTRE INDICII PRIMELOR MOSTRE NU ESTE SINGURĂ O DOVADĂ DE SKEW: dacă un DAC pornește mai târziu, trebuie comparat și momentul de prezentare. În JSON există doar un contraexemplu construit, cu ipoteze explicite, nu o măsurare a DAC-urilor utilizatorului.

În interiorul unui singur DAC, canalele folosesc aceeași poziție fracționară. Problema A/B nu explică automat o diferență între două căi care sunt ambele pe DAC A. Maparea fizică actuală este necesară.

Un alt punct: ramura de recenter mută readPosition, incrementează resyncCount și apoi finalul `render` raportează LOCKED. Hostul nu publică resyncCount. Astfel `xruns=0` nu certifică singur absența tuturor salturilor de cronologie. Nu s-a demonstrat că această ramură se execută în audiția reclamată.

ClockSync rămâne sinc polifazat 96 taps; convertorul rămâne flat TPDF. Nu există motiv demonstrat pentru a le înlocui cu alt interpolator.

## 7. Medii/midlow — cel mai relevant indiciu concret

`validation/stage013/actual-lanes.csv` este o CONFIGURAȚIE ISTORICĂ PUBLICATĂ, nu o citire a profilului live de astăzi. În ea:

- CH1/2/3 și CH5/6/7 au `allPass=1`, `apFreq=1000`, Q≈0,71.
- Center CH4 are `allPass=0`.
- WAY1 este HP80,1 / LP2040,6 Hz; WAY2 este HP775,6 / LP8604,7 Hz. Nu formează un split electric LR24 complementar la o singură frecvență comună; poate fi intenționat acustic.
- SUB CH8 folosește FIR4097 și un all-pass la 110,4 Hz.
- SCOOP CH12 avea HP/LP oprite; utilizatorul poate să fi corectat între timp acest lucru.

`allPass` manual este DIFERIT de `fractionalDelayMode` Thiran. All-pass-ul manual de ordinul 2 are modul unitar, dar la frecvența sa centrală transferul este −1, adică 180°. Aplicat comun pe toate căile L/R, nu le modifică faza RELATIVĂ între ele; modifică însă relația L/R față de un Center fără același AP și față de alte ramuri suprapuse. Nu rezultă automat o anulare: trebuie incluse gain-urile, delay-urile, filtrele, extracția Center și transferurile acustice.

Stage2 include acum EQ MUSIC LR pe un tap diferit de Center, care primește LR raw. Dacă EQ este activ, trebuie inclusă și această diferență în comparația de fază/transfer. Nu presupunem că este activ sau defect.

Atenuarea interpolării liniare de jumătate de sample este doar −0,001163 dB la 1 kHz / 192k. Revenirea înaltelor cu Thiran este plauzibilă, dar nu dovedește că schimbarea lui în toate benzile repară o problemă mare de medii. Se păstrează Thiran și se verifică transferul complex și continuitatea pe căile efectiv folosite.

## 8. PCA, clipping și Headroom — ce este și ce nu este rezolvat

PCA nou utilizează subbenzi prin diferențe de low-pass plus rezidual. În cazul coerent cu ponderi egale, reconstrucția se telescopiează la intrare. Dovezile publicate arată eliminarea câștigurilor coerente mari și identitate a mostrelor directe L/R/C față de referință în comparația PCA.

Același `stage015/numeric.json` păstrează un replay cu 1310 mostre overrange / 16 blocuri clipped pe DAC A, peak1,47956, în timp ce SUB are peak0,813. Era modul Headroom 1 (BIT PERFECT), cu diferență pre/post Headroom zero. Nu este o probă că AUTO a comprimat acel fragment. Nu este nici o probă că audiția actuală încă clipește: raportul live-switch 0.1.5 arată contoare zero pentru ambele DAC-uri în intervalele lui. Ambele fapte trebuie păstrate.

Masca detectorului a fost corectată, iar configurarea grupurilor incompatibile este respinsă explicit. Nu mai recomandăm repararea încă o dată a acelor puncte.

Rămâne asimetria din `AudioGraph::process`: `analyzeFinal()` este apelat doar în MUSIC. AUTO în CINEMA nu urmărește vârfurile blocului curent. Starea de gain poate fi modificată de schimbarea ledger-ului, deci nu afirmăm că rămâne obligatoriu la o anumită valoare; afirmăm precis că detectorul final nu este rulat pentru CINEMA. Trebuie stabilită o politică explicită, nu o aparență de AUTO universal.

## 9. Domeniul afirmațiilor

Confirmat static: benchmark pe comenzi, coadă serială, reset la re-ARM, erori de grafic, lipsa contractului temporal A/B, analiză finală numai MUSIC, sursă pasivă V2, masca Headroom reparată.

Calculat separat: factorul FFT +6,0206 dB, deformarea axei, duratele în mostre, droop linear. Nu am executat DLL/EXE/IPP/WASAPI.

De confirmat local: profilul actual, mod Headroom și EQ, rutarea fizică, apariția resync, skew A/B măsurat și prima etapă care schimbă neintenționat componenta mid/midlow. Nicio îmbunătățire auditivă nu este certificată prin acest audit.


## Surse inspectate

- [`Engine/Control/LiveEligibility.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/LiveEligibility.cpp)
- [`Engine/Control/CoreRuntime.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/CoreRuntime.cpp)
- [`IPC/CommandServer.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/IPC/CommandServer.cpp)
- [`UI/Client/IpcClient.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Client/IpcClient.cpp)
- [`Engine/AudioScheduler.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/AudioScheduler.cpp)
- [`Engine/Egress/LiveSinkWorkers.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/LiveSinkWorkers.cpp)
- [`Engine/Egress/NativeEgress.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/NativeEgress.cpp)
- [`Engine/Egress/SpscAudioFifo.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/SpscAudioFifo.h)
- [`Egress/ClockSync/src/EgressClockEngine.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Egress/ClockSync/src/EgressClockEngine.cpp)
- [`Egress/WasapiSink/src/WasapiDacSink.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Egress/WasapiSink/src/WasapiDacSink.cpp)
- [`DSP/Crossover/compat/src/CrossoverModule.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/compat/src/CrossoverModule.cpp)
- [`DSP/Crossover/include/IppSpectrum.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/include/IppSpectrum.h)
- [`DSP/Crossover/include/RealtimeFir.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/include/RealtimeFir.h)
- [`UI/Legacy/R10/CrossoverPage.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Legacy/R10/CrossoverPage.cpp)
- [`UI/Client/EngineFacade.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Client/EngineFacade.cpp)
- [`Engine/Control/CrossoverAccess.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/CrossoverAccess.cpp)
- [`DSP/Crossover/src/PluginProcessor.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/src/PluginProcessor.cpp)
- [`DSP/Headroom/src/HeadroomEngine.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Headroom/src/HeadroomEngine.cpp)
- [`Engine/AudioGraph.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/AudioGraph.cpp)
- [`Engine/Control/ControlPlane.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/ControlPlane.cpp)
- [`Transport/StagePublishV2/StagePublishV2.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Transport/StagePublishV2/StagePublishV2.cpp)
- [`Integration/Stage1/source/Host/EntropySourceEngine/Core/SourceController.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Integration/Stage1/source/Host/EntropySourceEngine/Core/SourceController.cpp)
- [`Integration/Stage2/source/Host/EntropyDspEngine/Core/DspStageRuntime.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Integration/Stage2/source/Host/EntropyDspEngine/Core/DspStageRuntime.cpp)
- [`validation/stage013/actual-lanes.csv`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage013/actual-lanes.csv)
- [`validation/stage015/numeric.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage015/numeric.json)
- [`validation/stage016/live-switch.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage016/live-switch.json)

## Referințe tehnice primare

- [Microsoft — IAudioClock::GetPosition](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition): poziție de stream asociată cu QPC, unități și calitatea citirii S_OK/S_FALSE.
- [Julius O. Smith — Thiran Allpass Interpolators](https://www.dsprelated.com/freebooks/pasp/Thiran_Allpass_Interpolators.html): all-pass și întârziere de grup maxim plată la DC, nu întârziere ideală pe întreaga bandă.
