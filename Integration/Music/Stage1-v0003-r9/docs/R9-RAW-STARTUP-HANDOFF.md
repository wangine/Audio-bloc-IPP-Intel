# Stage1 r9 — rectificarea disponibilității MUSIC RAW

Data: 2026-10-12. Raport suplimentar; `REPORT.md` păstrează diagnosticul anterior rectificării. Nicio operație Git efectuată.

## Prima frontieră identificată

În captura rece r8 `shared-r4`, FIFO-ul MUSIC avea zero cadre la tick-ul QPC **46943537624**. Pachetul continuu 15 (480 cadre PCM16/48 kHz, firstSourceFrame 7200) a terminat procesarea la **46943541104**, cu **0,348 ms** după tick. Nu există ruptură de secvență/cadre, lostFrames sau corupere RAW raportată. Stage1 a publicat un bloc invalid, zero, de 1024 cadre (5,333 ms), și a incrementat underrun. Evenimentul este la 156,145 ms după primul semnal RAW, după începutul ieșirii valide; este inclus în totalul interior, chiar dacă precede guard-ul statistic de 250 ms.

Replay-ul offline cu timpii reali de finalizare a ingress-ului reproduce exact același tick pentru r8. Cu politica r9, aceleași pachete și aceiași timpi produc zero goluri și zero underrun. QPC din descriptorul RAW este momentul commit-ului în driver, nu momentul livrării către Stage1.

Aceasta localizează golul de disponibilitate la consumatorul FIFO Stage1 și rezerva sa de pornire. Nu demonstrează identitatea PCM livrat de TIDAL față de un FLAC original și nu atribuie diferențele de timbru dintre Shared/Exclusive acestei erori. Comparația cu fișiere originale rămâne etapa următoare.

## Schimbarea locală

Candidat separat: `D:/Projects/3Stage-3/Stage1-v0003-r9`. Referința r8 rămâne intactă. Singura schimbare de comportament este în `Engine/SRC/SourceDomain.cpp`: pentru pachete `RAW_IDENTITY`, pragul de pornire devine două perioade de pachet plus un bloc de ieșire, rotunjit în blocuri de 1024 și limitat la capacitatea FIFO existentă (16384 cadre). AUX legacy păstrează formula anterioară.

Pentru pachete de 480 cadre la 48 kHz, pragul crește de la 3072 la 5120 cadre FP64/192. Diferența nominală a rezervei este 10,667 ms; întârzierea efectivă depinde de packetizare, startup-ul SRC și faza tick-ului. În replay-ul aceleiași capturi r4, pornirea r9 este cu **21,2054 ms** mai târzie. Nu este o promisiune de latență fixă de 10,667 ms. Pentru pachetele Exclusive reale de 2048 cadre la 44,1 kHz, pragul r9 atinge limita existentă de 16384 cadre (85,333 ms de rezervă).

Conversia PCM, r8brain, filtrele, Matrix, kernelul 0.3.0.11, providerul RAW de producție, Stage2, Stage3 și UI nu au fost modificate. Nu se schimbă nivelul, eșantioanele, ordinea pachetelor sau numărătoarea starvation RUN. Deadline-ul existent păstrează drenarea streamurilor scurte.

## Verificări

Testul nou folosește numai timpii capturii, cu PCM stereo sintetic distinct pe canale; nu include PCM din piesă. A eșuat înaintea rectificării cu un gol la același moment și trece după rectificare. Cele **19 teste native** trec: cele șapte existente și 12 combinații PCM16 / PCM24-valid-in-32 × 44,1 / 48 / 88,2 / 96 / 176,4 / 192 kHz. Sunt comparate bit cu bit eșantioanele SRC, drenarea completă a blocurilor la IDLE și raportarea starvation RUN real. Cadenta sintetică de 10 ms nu reprezintă toate packetizările Exclusive; acestea sunt acoperite separat prin proba live de mai jos.

| Captură | RAW efectiv | Acces verificat | Cadre FP64 valide comparate | Mismatch | Blocuri invalide interioare | Underrun final |
|---|---|---|---:|---:|---:|---:|
| shared-r4 | PCM16/48000 | S_OK | 2976768 | 0 | 1 | 1 |
| shared-r5 | PCM16/48000 | S_OK | 2970624 | 0 | 0 | 0 |
| exclusive-16441-r2 | PCM16/44100 | AUDCLNT_E_DEVICE_IN_USE | 2968576 | 0 | 0 | 0 |

R9 live: Shared 16/48 kHz și Exclusive 16/44,1 kHz, 15 secunde de analiză fiecare, circa 15,5 secunde de semnal capturat. Shared Initialize a returnat S_OK; Exclusive a refuzat Initialize(SHARED) cu 0x8889000A / AUDCLNT_E_DEVICE_IN_USE, cu TIDAL activ. Clientul de diagnostic nu a pornit redare.

În ambele capturi r9: zero rupturi de pachete/cadre, lostFrames, overrun, underrun, fault, publisher drops și clock late blocks. Zero blocuri invalide interioare; zero blocuri valide complet zero în fereastra continuă. Trace-ul acoperă integral fereastra. Observer overflow, failed writes și output subscriber errors sunt zero.

SRC-ul replay-ului este biblioteca originală r8. Toate cadrele valide publicate de r9 coincid bit cu bit, pentru ambele canale. Cozile reference încă nepublicate la oprirea intenționată a Core-ului sunt documentate în analysis.json; nu sunt certificate drept pierderi în redarea continuă, nici omise din raport. Testele IDLE verifică explicit drenarea blocurilor complete.

### Toate blocurile zero/invalide

Blocurile de așteptare/startup sunt tot invalid/zero conform contractului existent; nu sunt tratate drept PCM valid și nu sunt ascunse prin filtrarea statisticilor. Politica de publicare a acestora nu a fost schimbată.

| Captură | Invalid/zero total | Așteptare înainte de primul RAW nenul | Startup înainte de prima ieșire validă | Valid/zero total | Coada SRC nepublicată la stop (cadre) |
|---|---:|---:|---:|---:|---:|
| shared-r4 | 52027 | 52016 | 10 | 0 | 2308 |
| shared-r5 | 8904 | 8890 | 14 | 0 | 4612 |
| exclusive-16441-r2 | 5190 | 5156 | 34 | 1 | 16862 |

## Revizie și limite

Revizia independentă read-only nu a găsit regresii Critical/Important/Minor. A confirmat că numai politica SourceDomain diferă în Engine și că ThirdParty/SRC, ABI, UI, PCM decode și Matrix rămân neschimbate. Revizia a inspectat dovezile testelor; nu le-a rerulat.

Validarea privește ferestrele, modurile și formatele documentate; un FIFO finit nu garantează continuitate la întârzieri arbitrare sau identitate cu sursa FLAC fără acea sursă. Pentru noua comparație FLAC trebuie aliniat PCM cunoscut înainte de SRC, apoi verificată ieșirea cu conversia și SRC-ul de referință; pentru intrări sub 192 kHz, eșantioanele resamplate nu sunt identice sample-for-sample cu PCM-ul inițial.

## Fișiere și runtime

Runtime local: `Stage1-v0003-r9/release/stage1-native-music-v0003-r9`. Providerul RAW de producție și toate DLL-urile/UI sunt copiate identic din r8; numai Core-ul este construit cu politica r9. Configurația selectează RAW Music Render V2 și AUX legacy fixat la Entropy AUX / system default.

Pornire: `Start-Stage1-Music.cmd`; restart Core: `ColdStart-Stage1-Music.cmd`; oprire: `Stop-Stage1-Music.cmd`. Nu porni simultan r8 și r9 pe același contract; folosește stop-ul launcherului care deține Core-ul. Ordinea Stage1/2/3 nu este impusă.

Uneltele, PCM-ul capturat și rapoartele sunt păstrate în acest director Diagnostics. `SHA256SUMS.json` este inventarul de integritate; fișierele de stare ale unui runtime activ pot evolua. Raportul inițial rămâne separat.

### Verificarea runtime-ului normal

Core r9 pe contractul normal, monitorizat 59.3 s prin control IPC, 59 snapshots. Acest monitor nu este un cititor RAW și nu certifică singur absența tuturor blocurilor zero; acestea au fost numărate în capturile de mai sus.

- MUSIC: state=2, rate=44100, representation=1, packets=1284, delta packets=1275, underruns=0, faults=0, lostFrames=7653376.
- AUX: state=1, rate=0, representation=0, packets=0, delta packets=0, underruns=0, faults=0, lostFrames=0.
- Publisher drops=0; clock late blocks=0.

**Reconectarea nu este certificată zero-loss:** lostFrames a crescut cu 7653376 în prima monitorizare. TIDAL a continuat în aceeași generație cât nu exista cititor Stage1. Intervalul footer diagnostic → pornire Core normal este 179,048 s; 7.653.376 cadre raportate reprezintă 173,546 s la 44,1 kHz, iar ring-ul ready existent păstrează maximum 262144 cadre (5,944 s). Suma este compatibilă cu absența cititorului plus întârzierile de conectare. Contorul apare după aproximativ 6,4 s de citire și rămâne constant până la finalul monitorizării. Este o inferență susținută de capacitatea ring-ului și de cronologie, nu o dovadă RAW packet-by-packet în această reconectare.

Reverificare fără restart, 29.7 s / 30 snapshots: delta={'packets': 636, 'framesIn': 1302528, 'framesOut': 5670872, 'lostFrames': 0, 'overruns': 0, 'underruns': 0, 'faults': 0}. Contorul anterior nu a fost șters sau mascat. Această probă de telemetrie nu înlocuiește captura completă pentru certificarea fiecărui bloc publicat.

