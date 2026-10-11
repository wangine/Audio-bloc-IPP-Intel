# TIDAL real și AUX — 2026-10-11

Driverul încărcat este **0.3.0.11**, SYS SHA256
`FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA`.
Stage1 v0003-r3 și providerul raw au hashurile candidate deja calificate.
[Datele și sumarul regenerabil](../../Integration/Music/Stage1-v0003/evidence/tidal-v2-real-20261011/LIVE-RESULT.json)
leagă fiecare fereastră de JSONL-ul său prin SHA256.

Utilizatorul a confirmat TIDAL pe Music Render V2, Exclusive. Prima fotografie
actuală arăta deja providerul raw; selecția identică a fost redeschisă prin IPC.
MUSIC a primit **PCM16/44100**, RAW_IDENTITY, generația 65. AUX a păstrat aceeași
selecție Windows Capture Shared, generația 1. Stage2 și Stage3 nu au fost modificate.

| Fereastră | Durată observată | Snapshoturi | Semnal simultan |
|---|---:|---:|---:|
| Prima, înainte/în timpul pornirii Spotify | 179,359 s | 352 | 51 |
| TIDAL + Spotify pornit | 179,625 s | 353 | 352 |

Ambele ferestre au stări RUNNING, generații constante, fără creșteri ale
contoarelor fault/underrun/overrun și fără lostFrames raportate. În fereastra
simultană au avansat **7.925.760 cadre MUSIC** și **7.925.211 cadre AUX**.
Contorul MUSIC avea **540 underrunuri cumulate înaintea probei**, și a rămas 540.
Acestea nu sunt șterse, explicate sau atribuite unui anumit traseu prin acest test.
Contorul AUX a rămas zero.

## Formatul AUX și stereofonia rămân probleme deschise

Setarea endpointului Windows AUX Render este **24-valid-in-32/96000**, confirmând
informația utilizatorului. AUX Capture are setarea de dispozitiv **16/44100**,
iar clientul Stage1 deschis pe mix-ul de 44,1 kHz primește **24-valid-in-32/44100**.
Mix-ul render întors de GetMixFormat în proba actuală este tot 44100, deși setarea
persistată render este 96000. Nu s-a modificat nicio setare Windows pentru test.
Nu certificăm transport AUX original 24/96 sau păstrarea preciziei originale.

Toate măsurătorile AUX din fereastra simultană au avut L/R RMS identice și Side RMS
zero, deja **înainte de SRC**. MUSIC a avut Side nenul. Cauza nu este localizată.

Două probe Shared de 12 s, cu amplitudine 0,001 (**−60 dBFS**), au injectat
stereo anti-phase, apoi control pozitiv L-only. Ambele loopback-uri Windows au
livrat numai zero; captura AUX a rămas fără Side. Fără un martor independent că
semnalul cunoscut a ajuns în captura AUX, acest test de localizare este
**NECONCLUDENT**, nu dovadă de downmix în driver sau de defect Spotify.
Sursa probei și înregistrările sunt păstrate; probele se opresc singure.

## Limitele rezultatului

Continuitatea este verificată prin validările PCM raw din Stage1 și contoarele
observate în aceste ferestre finite. Acesta nu este un audit byte-for-byte al
PCM-ului TIDAL original și nu demonstrează echivalența Shared/Exclusive a TIDAL.
Metadatele raw kernel raportează mode=UNKNOWN; Exclusive este confirmarea
utilizatorului, nu o certificare prin acel câmp. UNKNOWN nu a produs fault în probă.

RMS L/R și Mid/Side sunt salvate înainte și după SRC, dar ferestrele lor instantanee
diferă. Nu calculăm din ele un gain sau o modificare stereo ca și cum ar reprezenta
aceleași eșantioane. Maximele din sumar sunt ale snapshoturilor observate, nu
maxime continue ale întregii redări. Rezultatele sintetice PCM/FP64 anterioare și
arhivele semnate rămân neschimbate.

## Tranziții ulterioare: r3 nu este calificat ca sesiune completă fără underrun

După ferestrele PASS, MUSIC a trecut din generația 65 în 67 și din 16 în 24
biți; contorul cumulativ a crescut de la 540 la 683. Nu avem o captură a acelui
moment suficientă pentru atribuirea celor 143 de incrementări.

Proba controlată următoare a capturat 2.127 snapshoturi Core în 239,828 s și
2.235 snapshoturi driver. Utilizatorul a efectuat trei pauze/reluări TIDAL de
3–6 s. Core r3 a crescut de la 683 la 2.444 underruns (+1.761), în timp ce
driverul a raportat inclusiv stream absent (86 snapshoturi), lostFrames=0,
lifetimeLost neschimbat și writeFailure neschimbat. Core nu avea încă metadata
de IDLE și număra golirea FIFO în pauză drept starvation. Această eroare de
contorizare este reprodusă separat în regresii, fără presupunerea că toate
incrementările istorice au aceeași cauză.

LifetimeCancelled a crescut cu 16.384 cadre la STOP; anularea este păstrată
separat și nu este prezentată drept EOS fără pierdere. Datele se află în
`Integration/Music/Stage1-v0003/evidence/tidal-v2-real-20261011/TRANSITION-RESULT.json`,
împreună cu capturile și sumarizatorul reproductibil. Candidatul r4 și Providerul
usermode r3 corectează IDLE/RUN/UNKNOWN fără schimbarea kernelului .11 sau PCM/SRC;
rezultatele sale live trebuie citite separat.
