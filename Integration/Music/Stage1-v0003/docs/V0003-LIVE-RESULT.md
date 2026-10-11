# Stage1 v0003 — verificări din 11 octombrie 2026

Referința v0002 este păstrată. Runtime-ul activ nou este `stage1-native-music-v0003-r3`, cu Core separat de Qt. Stage2 v0009 și Stage3 nu au fost schimbate. Driverul semnat încărcat este Music Render V2 0.3.0.11.

## Cauze reproduse

- Providerul V2 aștepta grosier, apoi Core aștepta încă o dată: în medie circa 31 ms pentru un ciclu fără date, deși blocul publicat durează 5,333 ms. Așteptarea providerului este acum anulabilă, cu timer precis, iar Core nu dublează o citire care a așteptat deja. Workerul folosește MMCSS Pro Audio HIGH.
- Pachetele de intrare de 1920 cadre și ieșirile de 1024 cadre aveau faze diferite. Amorsarea cu blocuri complete folosește FIFO-ul existent, fără mărirea lui și fără repetarea mostrelor. Politica și latența sunt declarate în README.
- AUX aștepta un eveniment înainte de fiecare citire și folosea GetNextPacketSize inclusiv în Exclusive. Acum apelează GetBuffer și drenează pachetele disponibile înainte de a aștepta. ReleaseBuffer este verificat. Un salt real de poziție rămâne fault; nu se repară prin inventarea cadrelor.
- Publisherul vechi transforma atât terminarea normală, cât și retragerea în starea 0. Publisherul v0003 folosește terminalul durabil 3; auditul drenează toate blocurile finale și refuză sesiuni retrase/eronate.
- Serverul de control deconecta pipe-ul imediat după WriteFile; un client întârziat putea pierde răspunsul. Confirmarea de primire este limitată la două secunde și se anulează la shutdown. Clientul vechi poate închide pipe-ul după citire.
- Citirea telemetriei returna zerouri după patru încercări suprapuse cu writerul. Cache-ul protejat numai între cititorii de control păstrează ultima observație coerentă; writerul și schedulerul nu iau mutexul. Testul concurent înainte de fix a numărat 335.901 snapshoturi goale și 1.487 regresii ale contoarelor; după fix, zero. Cache-ul nu garantează prospețimea în timpul unei coliziuni.
- Kernelul 0.3.0.10 folosea KeDelayExecutionThread de 1 ms în EnterPassive la finalizarea unei citiri concurente. Proba a măsurat read16,1713 ms și gap1024cadre, deși generatorul avea maxcommit11,34 ms și PCM raw integral. 0.3.0.11 trezește cititorul la eliberarea gate-ului prin SynchronizationEvent; RT folosește în continuare numai TryEnter. Probele vechi eșuate se păstrează.

## Rezultate și limite

Cele 12 cazuri raw PCM16/PCM24-valid-in-32, șase rate 44,1–192 kHz, Event-Driven, compară 12.894.000 de cadre stereo pe kernelul încărcat 0.3.0.11. Pe Core r3, verificarea finală nativă 192 kHz compară 23.040.000 cadre FP64 fără neconcordanțe, goluri interioare sau erori de subscriber, cu terminare explicită. Proba r1 de 60 s/formă a comparat anterior încă 23.040.000 cadre; este consemnată separat și nu înlocuiește verificarea finală.

La alte rate, ieșirea SRC nu poate fi identică eșantion cu eșantion cu intrarea. Testele diferențiale r8brain, nivelurile, canalele separate și vârfurile reconstruite rămân verificate fără clipping sau gain ascuns. Blocurile invalide de la start/stop sunt numărate separat de golurile din semnalul valid.

Fluxurile utilizatorului TIDAL + Spotify AUX Shared au fost urmărite timp de 300 s, cu polling Qt și citire de control întârziată 25 ms: zero faulturi/underrunuri raportate de Core. MUSIC era încă adaptorul legacy; nu certificăm PCM original pentru acesta. AUX Exclusive a trecut 120 s cu MMCSS, 5.292.000 cadre fără salturi de poziție. Proba anterioară fără MMCSS a eșuat la un salt de 1323 cadre și se păstrează. Nu atribuim acea diferență automat unui bug kernel și nu calificăm orice încărcare a mașinii.

Core: 6 CTest; driver/provider: 12 CTest; publicare terminală: 9 cazuri inclusiv două blocuri necitite la oprire. Launcherele au probă izolată de start repetat, cold start, păstrarea configurației și refuzul unei identități PID schimbate. Raportul numeric este `evidence/V0003-ACCEPTANCE.json`.

Polling nu are cale de commit certificată și este refuzat explicit. Nu există comparație care să demonstreze că TIDAL trimite același PCM în Shared și Exclusive. Certificarea live este pentru modurile, formatele, binarele și duratele consemnate, nu o promisiune de funcționare în orice condiții.

Documentele `LIVE-RESULT.md` și `DRIVER-V2-HANDOFF.md` copiate în acest director descriu etapa anterioară v0002. Pentru situația curentă se folosesc acest raport și documentele de arhitectură din repository.
