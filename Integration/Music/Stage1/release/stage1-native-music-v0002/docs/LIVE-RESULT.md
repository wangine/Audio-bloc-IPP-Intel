# Verificare Stage1 Native MUSIC/AUX — 2026-10-10

Faza B Core + client IPC separat, candidat local. Driverele și profilurile ascultate nu au fost schimbate; instanțele de test folosesc pipe-uri și state proprii. Decoderul și Stage3 nu au fost modificate.

| Verificare | Rezultat |
|---|---|
| MSVC Core fără Qt/JUCE/WDK | PASS build; importuri PE verificate |
| PCM16/24-in-32 și metadate stricte | PASS Stage1Pcm |
| Discovery/selectare exactă, ABI incompatibil/duplicat | PASS Stage1Architecture |
| Core independent de UI/Stage2; IPC versionat și mute refuzat | PASS Stage1Architecture |
| MSVC Stage1 → Stage2 MUSIC v0009 MinGW, DLL-uri native, restart | PASS Stage1Architecture |
| SRC pin-uit comparat numeric, generații/FIFO/discontinuități | PASS Stage1Pipeline |
| 48 fixture de nivel; identitate exactă la 192k | PASS Stage1Pipeline |
| Sample Peak/True Peak estimat/Startup separate, reset/settling | PASS Stage1Pipeline |
| Procesare încălzită, 100 blocuri, C++ new/delete | PASS; nu audit global Win32 heap |
| Qt MinGW offscreen conectat la Core MSVC, captură vizuală inspectată | PASS; evidence/qt-smoke.png |
| Captură reală TIDAL / Music Transport | PASS observație 35 s, 16/44,1, detalii mai jos |
| Spotify / WindowsCapture | NOT RUN |
| Shared/Exclusive, driver kernel, PCM commit identity | NOT RUN |
| Continuitate ore întregi / clock drift hardware | NOT RUN |
| Driver V2 build/signing/install | NOT RUN; proiect ulterior |

Queue StagePublish bounded: un consumator de test care nu citește la timp poate primi GAP. Testele raportează și reconectează consumatorul, păstrează UUID-ul producătorului și cer opt blocuri valide consecutive după conectare. Nu se pretinde că această reconectare înseamnă audio continuu fără pierdere. O execuție intermediară a detectat GAP și a eșuat înainte de corectarea comportamentului consumatorului de test; implementarea transportului pin-uit nu a fost modificată.

Warnings: C4324 pentru padding intenționat cache-line în FIFO; Qt configure raportează WrapVulkanHeaders absent (UI Widgets nu folosește Vulkan). Nu există test de conformitate ITU-R BS.1770 pentru estimatorul 4x. Auditul este direct, fără reviewer subagent, conform alegerii utilizatorului.

Hashurile exacte și proveniența componentelor sunt în manifestul/SHA256SUMS al pachetului. Pachetul local are selecțiile goale și nu a fost activat pe lanțul ascultat.

Packaged verification: three focused cases PASS with STAGE1_ACCEPTANCE_CORE pointing to the delivered executable. Packaged Qt twice connected/closed/restarted while the same Core kept publishing; tested without Qt/toolchain directories on PATH. Runtime checksums PASS; no Matrix/SRC DLL imports in Core. Evidence: packaged-verification.json, packaged-ctest.log, native-imports.txt.

## Verificare live și corecții v0002

Utilizatorul a oprit vechile Stage1/DSP și a pornit TIDAL în Exclusive pe Windows Music Transport. Intrarea selectată exact: `legacy.music-transport` / `entropy-music-transport:main`, acces adaptor 8. AUX a rămas None. Driverul și setările TIDAL nu au fost schimbate. Modul Exclusive este raportat de utilizator; câmpul mode al adaptorului rămâne UNKNOWN, fără atribuire artificială.

Primul test a găsit două defecte: Qt ignora SELECT cât timp STATUS era în curs; apoi ingress-ul intra în fault după patru pachete, când SRC-ul umplea FIFO-ul la preluarea backlogului. Comanda este acum păstrată pe rol. Workerul aplică backpressure anulabil; FIFO-ul, schedulerul, SRC-ul și gain-ul rămân aceleași. Fixture offline `burst44` livrează opt pachete de 2048 frames la 44,1 kHz în rafală: reproduce fault înainte de corecție și trece după ea. Core-ul livrat trece toate cele trei cazuri; Qt trece IPC smoke și testul selecției reale.

Observație live: 68 snapshoturi / 34,88 secunde, intrare 16/44100 și ieșire 192000. +749 pachete, +6522 blocuri; zero creștere pentru lostFrames, underruns MUSIC, clockLateBlocks, zero fault/overflow, UUID stabil. lostFrames este un contor cumulativ al driverului: valoarea inițială mare include perioada fără cititor; nu este declarată pierdere zero de la începutul redării. Un underrun inițial precede umplerea FIFO. Testul nu validează redarea prin DAC sau continuitatea de ordinul orelor.

Primul snapshot activ a raportat Sample Peak maxim 1,0 (0 dBFS), True Peak estimat 1,06205 (aprox. +0,52 dBTP), SRC Startup Peak maxim 1,03217 (aprox. +0,28 dBFS). Valorile sunt măsurări ale unor ferestre diferite de muzică; nu sunt o probă numerică de gain între aceleași eșantioane. Testele de nivel offline validează separat nivelul unitar. Dovezi: `live-continuity-35s.json`, `live-continuity-summary.json`, `live-after-backpressure.json`, `live-fault-before-backpressure.json`, `packaged-live-fix-ctest.log`.


Continuare: sesiunea Core corectată a fost oprită controlat după 588,12 secunde (aproape 10 minute). Snapshotul final are state active 16/44,1, faults=0, overruns=0, clockLateBlocks=0, underruns MUSIC=1 (inițial), lostFrames=11687936, neschimbat față de primul snapshot activ. Evidență: live-patched-session-final.json. Activarea v0002 păstrează exact selecția; snapshotul ulterior confirmă active 16/44,1, fără pierderi/underrun-uri noi, UUID stabil. TIDAL și driverul nu au fost reconfigurate. Oprirea și repornirea candidatei introduc o perioadă fără cititor în contorul cumulativ al transportului; aceasta nu este inclusă în observația continuă.
