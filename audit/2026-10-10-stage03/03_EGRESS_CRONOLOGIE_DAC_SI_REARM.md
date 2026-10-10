# Etapa 03 — cronologie egress, re-ARM și oprire anulabilă

**Scop:** ARM/DISARM controlează numai egress-ul; ieșirile A/B păstrează o cronologie de prezentare explicită; un resync nu dispare din telemetrie.

## Defecte și limită de diagnostic

Scheduler restart resetează Crossover și abonamentul. A/B pornesc separat și aleg primul sample din umplerea FIFO locală (`writeFrame-target`). Nu există dovadă de aliniere comună doar fiindcă ambele raportează LOCKED. Nu s-a măsurat aici skew-ul hardware și nu se știe încă distribuția actuală a căilor între DAC-uri.

## Execuție, fără rescrierea interpolatorului

1. Menține Stage2Ingress și AudioGraph în funcțiune prin ARM/DISARM normal. Pregătește un candidat egress inactiv pe worker-ul de lifecycle. Publică schimbarea prin handoff bounded la graniță de bloc; nu executa `prepare()` asupra unui pipeline pe care graph thread-ul îl folosește. Resursele sunt retrase și distruse numai după confirmarea proprietarului. Păstrează reset local pentru gap/epoch real, nu pentru butonul DAC.
2. Adaugă un stop/cancel event per owner DAC. `waitBuffer` trebuie să poată aștepta evenimentul audio SAU anularea. DISARM închide gate-ul imediat, trezește owner-ul, iar Stop/Close/COM release rămân pe thread-ul lui. Nu închide handle-uri în timp ce sunt așteptate și nu ucide thread-ul. Raportează STOPPING până teardown se termină; interfața/IPC nu așteaptă sincron join-ul.
3. Păstrează FIFO extern și ASRC până la rezolvarea identității temporale. Adaugă sideband prealocat cu `epoch`, `firstGraphFrame`, `frames`; nu schimba payload-ul FP64. Split/merge de blocuri trebuie să conserve frame index. Contoarele locale ale DAC-ului nu înlocuiesc identitatea mostrelor din graf.
4. Pregătește/deschide ambele dispozitive înainte de a activa grupul. Definește un moment comun de prezentare în QPC și maparea fiecărui stream față de el. Include bufferul inițial SILENT, poziția device raportată de `IAudioClock`, frecvența lui și latența raportată/calibrată unde este necesar. Același firstGraphFrame la două momente diferite de Start NU înseamnă sincronizare.
5. În preroll, alege prima poziție din contractul de prezentare, nu independent din `writeFrame-target`. Limitează transferul inițial la necesarul calculat; nu sări arbitrar peste o cantitate diferită de mostre fiindcă alt thread s-a trezit mai târziu. Dacă termenul comun nu poate fi respectat, replanifică grupul cât este încă mut, nu porni asincron pretinzând aliniere.
6. În regim stabil, păstrează estimatorul și sinc ASRC separate per DAC. Poziția/raportul urmăresc aceeași cronologie de graf raportată la timpul hardware. Păstrează slew limit și corecție bounded; nu rezolva backlog-ul cu accelerări mari sau eliminări periodice de mostre. Distinge eroare de frecvență, eroare de fază și backlog în FIFO extern.
7. Expune din date deja calculate: `resyncCount`, `firstGraphFrame` al intervalului, epoch, fill extern/intern, vârsta datelor, referința QPC a prezentării și diferența A/B estimată. O citire de ceas degradată sau absentă nu se raportează drept aliniere verificată. „LOCKED” singur rămâne stare de servo, nu certificat acustic.
8. Într-un stream activ, dacă ar fi necesară ramura actuală de recenter/salt, semnalizează evenimentul exact. Politica de recuperare este locală: mute/fade sigur și re-preroll explicit; nu ascunde evenimentul prin revenirea imediată la LOCKED. Nu propaga fault în Stage1/2. Nu modifica profilul acustic pentru a absorbi un offset de startup variabil.

## Verificare minimă

Un scenariu determinist cu două sink-uri simulate, starturi decalate și ppm diferit, verifică aceeași identitate de graf la același timp de prezentare, fără drop/duplicate neanunțat. După primele cadre nenule, bugetul software propus pentru model este ≤1 sample de diferență între mapările de prezentare, nu o promisiune despre hardware. Simulează și un DISARM în wait: owner-ul este trezit, iar graful/abonamentul nu sunt resetate. Nu adăuga un soak test.

O măsurare analogică A/B poate valida DAC-urile numai dacă ambele ieșiri sunt capturate simultan în același domeniu ADC. Fără această probă, raportează „contract software verificat; skew analogic nemăsurat”. Nu seta același ppm pe ambele DAC-uri ca să faci să dispară eroarea din afișaj.


## Surse inspectate

- [`Engine/AudioScheduler.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/AudioScheduler.cpp)
- [`Engine/Control/CoreRuntime.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/CoreRuntime.cpp)
- [`Engine/Input/Stage2Ingress.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Input/Stage2Ingress.cpp)
- [`Engine/Egress/LiveSinkWorkers.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/LiveSinkWorkers.cpp)
- [`Engine/Egress/NativeEgress.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/NativeEgress.cpp)
- [`Engine/Egress/SpscAudioFifo.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/SpscAudioFifo.h)
- [`Egress/ClockSync/src/EgressClockEngine.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Egress/ClockSync/src/EgressClockEngine.cpp)
- [`Egress/ClockSync/src/EgressRatioController.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Egress/ClockSync/src/EgressRatioController.cpp)
- [`Egress/WasapiSink/src/WasapiDacSink.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Egress/WasapiSink/src/WasapiDacSink.cpp)
