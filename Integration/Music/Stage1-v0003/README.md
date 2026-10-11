# Stage 1 MUSIC/AUX v0003-r7

Core C++20 nativ, independent de Qt. MUSIC și AUX au conversie PCM→FP64, SRC r8brain și FIFO separate. Ieșirea este stereo FP64/192 kHz pentru Stage2 MUSIC L/R→L/R/C. Cele două surse active se însumează unitar; fără mute, gain, normalizare sau limiter. Cinema rămâne independent.

## Pornire

Livrarea locală activă este `release/stage1-native-music-v0003-r7`. După extragerea runtime ZIP, folosiți **Start-Stage1-Music.cmd** din folderul r7. **ColdStart-Stage1-Music.cmd** repornește controlat numai Core-ul propriu; **Stop-Stage1-Music.cmd** îl oprește. Închiderea Qt lasă Core activ. Profilul și selecțiile se păstrează; PID-ul și momentul creării se verifică înainte de orice oprire.

Profilul livrat selectează MUSIC raw Music Render V2 și AUX Windows Capture Shared. Endpointurile sunt locale acestei mașini. TIDAL este selectat pe V2 de utilizator; Spotify folosește ieșirea Windows implicită AUX. Testele cu PCM cunoscut nu certifică PCM-ul original TIDAL sau echivalența Shared/Exclusive.

## Rectificarea r7

r7 asociază metadata RUN cu generația pregătită, ca noul stream să nu pornească deadline-ul FIFO-ului vechi înaintea primului său PCM. Proba cu Core real reproduce defectul r6 și verifică după rectificare și starvation-ul autentic. Toate cele șase teste Core trec. [Raportul r7](docs/V0003-R7-GENERATION-RESULT.md) păstrează dovezile și limitele; kernelul, Providerul raw și algoritmii PCM/SRC sunt aceiași. Cele trei pauze/play TIDAL și tranzițiile următoare au o captură proprie r7.

## Rezultatul istoric r6

Pauza IDLE confirmată nu crește underrun; RUN și UNKNOWN fără PCM rămân detectabile după fereastra de reluare. Startup-ul SRC inițial este păstrat. Kernelul semnat .11 este neschimbat; numai Providerul usermode r3 adaugă metadata de lifecycle. Frozen v0002 trebuie să păstreze Providerul său original, deoarece nu acceptă noua capabilitate.

Cele 12 probe cu STOP/RUN compară 2.578.800 cadre stereo raw, fără neconcordanțe sau underruns până la ultimul pachet. Un contor RUN de după tail rămâne vizibil. Două probe de 20 s native 192 kHz compară 7.680.000 cadre exact binary64, fără goluri interioare și cu toate contoarele zero. Core 6/6, Provider 12/12, terminal 9/9, Qt și launchere verificate. [Raportul r6](docs/V0003-R6-LIVE-RESULT.md) păstrează duratele, hashurile și limitele; erorile candidaților intermediari r4/r5 rămân în evidence.

Qt afișează pentru V2 `Raw PCM · WASAPI access unverified`. Mode0 din kernel nu confirmă automat Shared/Exclusive; PCM brut sau Event-Driven nu permit această inferență. AUX își raportează modul clientului propriu. [Contractul și afișarea](docs/IDLE-AND-ACCESS-R4.md) explică extensia separată, fără modificarea ABI-ului înghețat.

Următoarele probe r3 sunt istorice; ele nu reprezintă calificarea r6 sau rezolvarea integrală a AUX.

## Rectificări și probe

v0003 elimină așteptarea dublă grosieră din ingress, folosește timer precis și MMCSS Pro Audio, drenează toate pachetele AUX înainte să aștepte evenimentul și păstrează formatul la fault. Cititorul de control păstrează ultima telemetrie coerentă când o actualizare se suprapune; cache-ul nu este accesat de threadurile audio. Schedulerul primește FIFO amorsat cu blocuri complete. Un gol real după amorsare rămâne underrun; nu se inventează PCM. SRC și nivelul nu sunt modificate.

Amorsarea are o latență declarată: `ceil(packetFramesConverted/1024)+1` blocuri, în FIFO-ul existent de 16 blocuri. Exemple: 1920 cadre la 192 kHz → 3072 cadre/16 ms; 2048 cadre la 44,1 kHz → 10240 cadre/53,333 ms, plus timpul propriu de stabilizare SRC. Schimbarea generației reamorsează separat sursa.

Publisherul marchează terminarea normală prin starea terminală 3, după oprirea workerelor. Toolul drenează coada finală și cere terminare fără erori. Controlul Qt confirmă primirea răspunsului înainte de deconectarea pipe-ului; regresia folosește citire întârziată și polling simultan.

Rezultate: Core 6/6 CTest; driver/provider 12/12; publicare terminală 9/9. Cele 12 formate Event-Driven PCM16/PCM24-valid-in-32 la 44,1–192 kHz au trecut comparația brută. La 192 kHz ieșirea FP64 este identică valoric cu PCM normalizat, fără cadre lipsă sau blocuri invalide interioare. La alte rate SRC schimbă numărul și valorile eșantioanelor: verificarea este diferențială față de r8brain, nu identitate PCM.

TIDAL + Spotify AUX Shared: 300 s fără faulturi/underrunuri raportate de Core. AUX Exclusive cu MMCSS: 120 s, 5.292.000 cadre fără salturi de poziție. Testul anterior fără MMCSS a eșuat și se păstrează. Aceste probe au durată finită și nu certifică orice încărcare a sistemului. Polling rămâne nesuportat de driverul V2; nu există fallback ascuns.

## Tooluri păstrate

`tools/Build-Stage1Music.ps1` compilează Core și rulează cele șase teste. Qt se compilează separat prin `Build-Stage1Qt.ps1`. `Stage1IngressProbe.exe` și `Stage1ControlProbe.exe` se produc în `build`. `tests/OutputTerminalTests.py` verifică cele nouă cazuri cu auditul și fixture-ul publisherului, fără dispozitive audio.

`tools/Run-Diagnostic-Candidate.ps1` cere pachet de driver cu manifest, repository, Core candidat și SHA256 explicit. `-RunLive -Servicing event -VerifyFp64 -Seconds 20` verifică întreg traseul nativ 192 kHz. Fără `-VerifyFp64`, verifică PCM brut pe toate cele șase rate. Fiecare probă are director nou; rezultatele vechi nu se suprascriu. Folosiți numai endpointul driverului V2 pentru generator. Toolul și executabilele de probă sunt păstrate și în runtime: `tools/Run-Diagnostic-Candidate.ps1`, `core/Stage1IngressProbe.exe`, `core/Stage1ControlProbe.exe`, `core/Stage1OutputPublisherFixture.exe`; auditul raw/FP64 este în pachetul driverului.

`tools/Package-Stage1Music.ps1` arhivează un runtime deja testat, sursele și rapoartele, într-un director nou. Driverul semnat 0.3.0.11 elimină suplimentar o întârziere rară de circa 16 ms la finalizarea citirii raw din kernel, prin trezire la eliberarea gate-ului. Arhiva sursă include dependențele fixture-ului din pachetul driverului. v0002 și pachetele r6/r7 rămân referințe istorice nemodificate. Detalii: `docs/V0003-DESIGN-AND-TEST-PLAN.md` și `evidence/V0003-ACCEPTANCE.json`.
