# Stage 1 MUSIC/AUX v0003-r3

Core C++20 nativ, independent de Qt. MUSIC și AUX au conversie PCM→FP64, SRC r8brain și FIFO separate. Ieșirea este stereo FP64/192 kHz pentru Stage2 MUSIC L/R→L/R/C. Cele două surse active se însumează unitar; fără mute, gain, normalizare sau limiter. Cinema rămâne independent.

## Pornire

Livrarea locală activă este `release/stage1-native-music-v0003-r3`. Dublu-click pe **Start-Stage1-Music.cmd** din acel folder. **ColdStart-Stage1-Music.cmd** repornește controlat numai Core-ul propriu; **Stop-Stage1-Music.cmd** îl oprește. Închiderea Qt lasă Core activ. Profilul și selecțiile se păstrează; PID-ul și momentul creării se verifică înainte de orice oprire.

TIDAL a rămas pe Music Transport legacy, iar Spotify pe ieșirea Windows implicită AUX Shared. Providerul nou Music Render V2 este disponibil în catalog. Testele lui folosesc un generator WASAPI separat și nu certifică identitatea PCM a adaptorului legacy sau echivalența TIDAL Shared/Exclusive.

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
