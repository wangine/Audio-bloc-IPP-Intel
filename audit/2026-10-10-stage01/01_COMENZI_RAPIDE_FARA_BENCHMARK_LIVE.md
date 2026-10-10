# Etapa 01 — ARM/DISARM și MUSIC/CINEMA fără benchmark live

**Scop:** elimină calculul de calificare din calea comenzilor și face DISARM admisibil imediat, fără modificarea algoritmilor DSP.
**Bază:** README și auditul acestui pachet. Un singur commit/candidat pentru această etapă.

## Defectul exact

`CoreRuntime` cheamă sincron `preflightCache.run()` la ARM și comutări. `liveProfilePreflight` execută 1940/2900 blocuri pe o copie a grafului, în Pro Audio CRITICAL. `CommandServer` serializează toate handler-ele, deci un DISARM poate sta în spatele acestei activități. Cache-ul este unic și include mult mai mult decât configurația DSP relevantă.

## Execuție

1. Scoate `liveProfilePreflight` și probele sintetice din call graph-ul normal ARM/program/telemetry. Mută calificarea cronometrată într-un executabil diagnostic separat, lansat explicit; nu o muta într-un worker care rulează automat în paralel cu muzica la aceeași prioritate.
2. Păstrează verificările reale înainte de armare: profil valid, identități DAC, format exact, routing, coeficienți finiți, headroom policy validă, limite FIR autorizate și configurație aplicabilă. Pregătește coeficienții și resursele pe control/preparation worker. Un profil încă nepregătit produce PREPARING explicit, nu audio pornit pe o configurație neverificată.
3. Refolosește configurațiile pregătite MUSIC și CINEMA după o cheie DSP: implementare, rată, layout, parametri efectivi ai scenei. Nu include selectedChannel, monitor FFT, gesturi UI sau momentul unei salvări în cheia kernelului. Modificările relevante invalidează numai configurația afectată.
4. Handler-ul trebuie să admită cererea și să publice identitatea ei fără lucru de durată. Un executor serializat de lifecycle/preparation aplică operația în afara cititorului de comenzi. Stările sunt distincte: ACCEPTED/PREPARING/ARMING/RUNNING/STOPPING/DISARMED/FAULT. Nu redenumi ACCEPTED în „applied”.
5. DISARM ridică un cancellation generation și închide imediat gate-ul de publicare. Anulează orice ARM vechi/pending. Nicio pregătire terminată ulterior nu poate reporni DAC-urile dacă generation nu mai corespunde. Nu goli arbitrar comenzile de profil deja confirmate.
6. Nu ține răspunsuri FFT mari sau editări intermediare redundante în fața DISARM. Păstrează ordinea comenzilor semantice și granițele gesturilor; permite coalescing doar acolo unde ultimul parametru înlocuiește în siguranță valori intermediare neaplicate. Schimbul de răspunsuri rămâne corelat și cu un singur writer IPC.
7. Înlocuiește scanarea integrală de dispozitive la fiecare ARM cu revalidarea identităților selectate și un cache invalidat de schimbări de dispozitiv. Enumerarea/revalidarea pot rămâne operații de pregătire; DISARM nu așteaptă terminarea lor. Nu folosi un cache pentru a ignora hot-unplug sau schimbarea endpointului.
8. Nu opri încă sursa și nu introduce vreo cerere de frame upstream. Fără modificări vizuale ale paginilor existente; se schimbă numai semantica/starea corectă a controalelor.

## Verificare minimă

Un scenariu scurt cu sink simulat: ARM → MUSIC/CINEMA → DISARM, inclusiv o pregătire deliberat lentă. Demonstrează că DISARM este admis înainte să termine pregătirea și că aceasta nu face auto-ARM ulterior. Contorul funcției de benchmark trebuie să fie zero în toate aceste comenzi. Verificarea normală a siguranței trebuie să respingă un format/routing invalid.

Pentru candidatul Windows, o singură secvență live autorizată sau simulată raportează separat timpul admitere, gate și teardown. Obiectiv software: admitere/gate fără pauze de secunde; țintă 50 ms în condiții normale, fără a prezenta durata driverului ca garantată. Nu este necesar un benchmark de 1000 blocuri ca să măsori trei timestamp-uri.

## Nu modifica

Crossover math, Thiran, PCA, Headroom math, PCM24, SRC, rate correction, profilul original. Rezolvarea stop-event și continuității grafului/DAC este în etapa 03.


## Surse inspectate

- [`Engine/Control/CoreRuntime.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/CoreRuntime.cpp)
- [`Engine/Control/LiveEligibility.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/LiveEligibility.cpp)
- [`IPC/CommandServer.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/IPC/CommandServer.cpp)
- [`UI/Client/IpcClient.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Client/IpcClient.cpp)
- [`Engine/Control/DeviceService.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/DeviceService.cpp)
- [`validation/stage016/live-switch.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage016/live-switch.json)
