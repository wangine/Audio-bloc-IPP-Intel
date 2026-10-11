# Audit live MUSIC — 2026-10-11

TIDAL a fost comutat între Shared și Exclusive pe ambele dispozitive existente: Entropy Music Transport și Entropy Audio Virtual Device. Stage1 v0002 și Stage2 MUSIC v0009 au rămas aceleași procese. Nu s-au instalat/modificat drivere, DLL-uri DSP, SRC, gain, mute sau Stage3.

## Rezultatul important

**Există un pachet inițial lipsă în calea WASAPI Exclusive cu notificări.** În toate cele 24 de teste ale acestei căi, bridge-ul a început cu al doilea bloc, deși primul fusese trimis prin `IAudioRenderClient::ReleaseBuffer` înainte de `Start`. Bufferul probei are 10 ms: 441 / 480 / 882 / 960 / 1764 / 1920 cadre, în funcție de rată. `lostFrames=0` nu detectează această lipsă: contorul urmărește suprascrierea ringului, nu un pachet respins înaintea activării ownerului.

După acel început, toate cadrele capturate sunt identice cu secvența PCM cunoscută. Nu s-au găsit eșantioane corupte, duplicate, inversări de ordine sau goluri în interiorul secvenței. Cele 24 de teste Exclusive cu polling încep chiar de la cadrul 0 și trec inclusiv verificarea pornirii.

Acest rezultat **nu explică încă reseturile semnalate de utilizator în mijlocul unei piese**. În observațiile live stabile acestea nu au fost reproduse. Comutările deliberate Shared/Exclusive au produs generații noi și pauze de intrare, fără repornirea Core-ului.

## Matricea PCM cunoscută

| Dispozitiv | Cale WASAPI | Formate | Rezultat |
|---|---|---|---|
| Music Transport | Exclusive, notificări | 6 rate × 16/24 biți | FAIL la primul bloc; secvență ulterioară exactă |
| Virtual Device | Exclusive, notificări | 6 rate × 16/24 biți | FAIL la primul bloc; secvență ulterioară exactă |
| Music Transport | Exclusive, polling | 6 rate × 16/24 biți | PASS, inclusiv primul cadru |
| Virtual Device | Exclusive, polling | 6 rate × 16/24 biți | PASS, inclusiv primul cadru |

Rate: 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz. Stereo PCM16 în 16 biți și PCM24 MSB-aligned în container32. Fiecare caz are aproximativ 3 secunde. Total: **48 cazuri, 15.453.202 cadre stereo comparate exact** după intervalul de pornire specific fiecărei căi. Zero coruperi, duplicate, goluri interioare sau pierderi raportate de ring în execuțiile finale.

Aceste comparații folosesc un client WASAPI și PCM sintetic cunoscut. Nu sunt o comparație byte-for-byte cu bufferul original intern al TIDAL, pe care adaptorul legacy nu îl expune ca raw PCM cu identitate de pachet.

Proba este un experiment de diagnostic, separat de produs, în `evidence/driver-probe`. TIDAL a fost pus pe pauză și rolul MUSIC al Stage1 a fost temporar None pentru a avea un singur cititor al bridge-ului. Utilitarul redă numai pe cele două endpointuri virtuale identificate exact după nume; nu deschide DAC-uri fizice. Perechile L/R sunt distincte și identifică univoc poziția din secvența generată. Bridge-ul livrează valorile Q31; comparația ține cont de reprezentarea exactă PCM16/24 în Q31. Coada încă nerenderizată la `Stop` nu este declarată pierdere de transmisie. Testul nu verifică drain/EOS complet sau funcționarea de ordinul orelor.

Sursele locale ale Music Transport explică mecanismul: `SetWritePacket` trimite prefill-ul către `EntropyBridgeSubmitPacket`, dar ownerul este activat abia la tranziția în RUN, în `SetState`. Submit-ul respinge pachetul dacă ownerul/streamul nu este activ. Fișierele examinate sunt în proiectul local de referință `D:/Projects/Entropy Codex Lucru/Modular 3 stages/source/Drivers/EntropyMusicTransport/src/EndpointsCommon/`. Aceasta este o explicație susținută de cod și de secvențele capturate; nu s-a construit sau instalat o corecție kernel. Pentru Virtual Device proba confirmă aceeași lipsă, fără a pretinde identificarea exactă a sursei buildului kernel instalat.

## Shared și referința Windows

Mixul Windows negociat este stereo FP32 / 44.1 kHz; driverul raportează PCM24/container32. Volumul endpointului: scalar1, 0 dB. PCM-ul original al probei nu este păstrat identic de mixul Shared. Acesta nu este declarat automat corupt.

Au fost capturate simultan ieșirea loopback a mixului Windows și bridge-ul, pentru ambele dispozitive și două secvențe de test (16 și 24 biți). După alinierea măsurată de **64 cadre**, comparația întregului interval comun arată maximum **128 unități Q31 = 0,5 LSB la 24 biți**, fără niciun eșantion în afara acestei limite. Prefixul bridge-ului dinaintea referinței este zero. Ultimele 64 de cadre ale referinței nu au corespondent în intervalul capturat și sunt raportate explicit, fără extrapolare.

Prin urmare, modificările mari față de secvența PCM originală există deja înainte de bridge, în mixul Windows. În intervalul comun verificat nu apare corupere suplimentară peste cuantizarea FP32→PCM24. Nu se afirmă că Shared este bit-transparent.

Verificare offline reproductibilă: `node evidence/driver-probe/Verify-Shared.mjs` din Stage1. Rezultat PASS; date și raport: `shared-loopback-verification.json`, capturile sintetice `*-captured-q31.bin` și `*-loopback-q31.bin`. Exit1 al probei Shared inițiale indică nepotrivirea cu PCM-ul original; acceptarea față de mix se face separat cu acest verificator.

## Observații live și Stage2

- Music Transport: generație 537→539 la Exclusive OFF, 16/44.1→24/44.1; apoi 539→541 la ON, revenire 16/44.1. Pierderi zero; underrun-urile de tranziție sunt păstrate în raport.
- Virtual Device: Exclusive stabil 35 s, generație5, fără pierderi/underrun-uri noi. Pierderile inițiale cumulative includ perioada în care TIDAL reda pe acest dispozitiv înainte de conectarea cititorului.
- Virtual Device OFF: generație5→7, 16→24 biți; ON: generație9/16 biți stabilă în următoarele20 s, fără pierderi noi.
- `access UNKNOWN` vine din adaptorul legacy Stage1: `LegacyProvider.cpp` setează explicit `ENTROPY_MODE_UNKNOWN`. Diagnosticul Music Transport raportează `mode=2` la Exclusive și `mode=1` la Shared. UI UNKNOWN nu dovedește reset sau mod Shared. Virtual Device nu oferă același câmp de mod în ABI-ul său de citire PCM.
- Prima cerere diagnostică de selectare Virtual Device a fost refuzată deoarece rolul IPC era0. A fost corectată la rol1, verificată în răspuns, apoi observația a fost repetată. Fișierele `live-virtual-initial*` și `live-virtual-selected.json` nu sunt dovezi de acceptare a Virtual Device; se folosesc `live-virtual-exclusive*` și `live-virtual-selected-valid.json`.
- Prima probă nativă avea `Sleep(1)` după fiecare citire, inclusiv la pachete pline. Granularitatea somnului a limitat consumatorul la rate mari. Versiunea finală drenează imediat pachetele pline și păstrează maximul pierderilor înainte de stop/reset. Execuțiile inițiale sunt arhivate în `probe-v1-with-consumer-pacing-limit` și nu sunt folosite drept defecte ale driverului.
- La restaurarea TIDAL, meniul a interceptat un clic și a selectat accidental Chromecast. Selecția a fost corectată imediat; restaurarea finală este confirmată din driver și din semnalul Stage2. `stage2-live-lrc3-subscriber.json`/`live-restored-continuity*` preced restaurarea corectă și nu validează semnal MUSIC; se folosesc fișierele cu `restored`/`confirmed` indicate mai jos.

Stare finală: **TIDAL → Music Transport Exclusive → Stage1 MUSIC v0002 → Stage2 MUSIC v0009**, AUX None. Driverul raportează activ, mode2, PCM16/44.1, generație631. Sesiunea Core a rămas `E197FE92FBFD2F4ABE3BB131E6C3BE53` pe durata tuturor schimbărilor de sursă.

Un subscriber pasiv a citit **1.873 blocuri consecutive** de la Stage2: layout4, 3 canale FP64/192 kHz, toate cu MUSIC valid; zero goluri de secvență, erori de transport sau eșantioane NaN/Inf. Peak L/R/C: 0,674826 / 0,513924 / 0,272671. Nu este conectat un DAC de această probă.

După restaurare: **490 snapshoturi /35 s**, generație631 constantă, același UUID, zero creștere pentru lostFrames și underruns, zero faults/overruns/clockLateBlocks. UI a revenit la `Core connected`. Un mesaj UI tranzitoriu `Core unavailable` în timpul sondajului concurent nu a coincis cu oprirea clock-ului/Core-ului; nu este atribuit driverului fără o probă suplimentară.

## Dovezi și componente

- `evidence/driver-probe/final-summary.json`: agregarea matricei finale.
- `evidence/driver-probe/*-exclusive-*-summary.json`: rezultate individuale, inclusiv FAIL de pornire.
- `evidence/driver-probe/shared-loopback-verification.json`: eroare/aliniere Shared.
- `evidence/stage2-live-lrc3-subscriber-restored.json`: ieșire L/R/C efectiv citită.
- `evidence/live-restored-confirmed-continuity-summary.json`: continuitate live finală.
- `evidence/live-reset-summary.json`: 841 snapshoturi/40 s, fără reset, pierdere sau underrun nou în interval.

Driverele instalate testate (SHA256):

- Music Transport: `3E631D65DD2322C1E9932A450D864065736C0900992A8839F9B5EF01BEB3DCDF`.
- Entropy Audio Virtual Device / TabletAudioSample: `F14A4C272977183E81491D0B33577D7A20EE3D78FBAEFCA25E75EB835A281F91`.

Limita acceptării: Stage1/Stage2 funcționează live în intervalele măsurate; bridge-urile existente **nu trec acceptarea completă a pornirii Exclusive cu notificări**. Remedierea trebuie făcută în ownership/commit-ul driverului, păstrând prefill-ul până la RUN și publicându-l o singură dată. Nu se compensează prin mute, gain, SRC sau ascunderea contoarelor. Resetul relatat în mijlocul unei piese rămâne nereprodus în aceste ferestre.
