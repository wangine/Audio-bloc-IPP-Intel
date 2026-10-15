# C2 — implementarea integrării ClockFollow MUSIC + AUX, apoi build izolat

Data: 15 octombrie 2026.

**Starea acestei livrări:** aceasta este specificația/taskul de IMPLEMENTARE pentru Codex. Commitul care adaugă documentul NU implementează C2, nu produce un Core nou și nu califică redarea live. C1 există deja ca actuator MUSIC compilat, dar nu este instalat. Nu prezenta acest document sau un build al vechiului Core drept integrare terminată.

## 1. Rezultatul cerut și limitele taskului

Implementează, în aceeași ramură C2 izolată, legătura dintre DAC-ul master, driverul MUSIC C1, driverul AUX, transportul Stage1 și ieșirea DIRECT_PRIMARY a Stage3. Construiește un candidat coerent cu aplicații/driver AUX și instrucțiuni de instalare ulterioară; NU instala și NU porni audio în această sarcină.

Livrabilul este cod integrat + build + pachet versionat. Nu te opri la un raport de identificare, clase neapelate, stubs, un simulator separat sau un script care compilează vechiul runtime. C2 poate necesita mai multe commituri locale coerente, dar rămâne un singur task. Nu extinde în paralel DSP-ul sau alte drivere.

Păstrează procesele Stage1/Stage2/Stage3 independente și controlabile separat. Independența nu înseamnă că toate pot ignora cronologia ori livrarea PCM. Transportul trebuie să raporteze lipsa datelor valide, nu să ascundă defecțiunea prin eliminarea/repetarea mostrelor, schimbarea gain-ului sau resetarea periodică a fazei.

Calea compatibilă LiveV1.1 rămâne disponibilă și implicită. Nu activa noua cale directă automat și nu elimina WDL global. CINEMA prin UR23 și DAC-ul secundar rămân domenii asincrone în acest task.

## 2. Surse de bază — nu relua căutarea

Repository țintă: `wangine/Audio-bloc-IPP-Intel`.
Ramură de lucru: `chatgpt/clockfollow-c2-integration-20261015`.
Baza aplicațiilor: `2264e5efb62e411a83432a2b41b9b6c5bde5df12`, care consemnează LiveV1.1 construit din `9ed04a1b0c71e061e41f32a1ec0d7be90724e3cd` și include `Integration/Music/Stage1-v0003-r9`.

C1 MUSIC: același repository, ref `1faae8979f03c32baf61861b921fbe0a46e64e51`, numai arborele `Drivers/EntropyMusicRenderV2` și dependențele lui efectiv necesare. Sursa binarului C1 este `3faccc678242f2815cc61eeb8f49a2f05c0c85b5`; diferența ulterioară este de împachetare. Nu face merge integral al ramurii C1 în ramura aplicațiilor: conține alte versiuni istorice de Stage1/3.

AUX: `wangine/Entropy-DSP-A06-independent-clock-control-`, ref fix `1c8336e722dbbe645bdb0d707f761d2e297cb5fc`:
- `Modules/AuxEndpoint/src/driver/`;
- `Modules/AuxEndpoint/installer/AUX-In/EntropyAux.inf`;
- adaptorul necesar din `Shared/EndpointBridge/` și headerele ABI pe care le include.

Importă numai componentele necesare din AUX într-un arbore izolat, de exemplu `Drivers/EntropyAuxClockFollow/`, cu manifest de proveniență și licențe. Păstrează originalul importat identificabil și diferențele de adaptare clare. Nu importa motorul A06, alte drivere, binare, chei sau SDK-uri.

### Identificarea locală deja furnizată de utilizator

- Stage1 r9: `legacy.aux`, `entropy-aux:system-default`, mode `4`.
- Configurație: `D:\Projects\3Stage-3\Stage1-v0003-r9\release\stage1-native-music-v0003-r9\state\Stage1Music.ini`.
- AUX render: `{0.0.0.00000000}.{cbd9235e-6e13-4663-8f0a-56927a262ab8}`; default Console/Multimedia/Communications în momentul raportului.
- AUX capture: `{0.0.1.00000000}.{f0d218fd-25cb-4db9-b2f9-e58e3ab46f7a}`.
- Părinte PnP: `ROOT\ENTROPY_ENDPOINT\0001`; hardware ID `Root\EntropyAudio_AuxInput`; serviciu `EntropyAudioAux`; INF instalat `oem15.inf`; versiune PnP `1.1.1.5`.
- SYS instalat: `C:\Windows\System32\DriverStore\FileRepository\entropyaux.inf_amd64_9be04f045972ce79\entropyaux.sys`; FileVersion `10.0.10011.16384`.
- SHA256 SYS AUX instalat: `E4235D1B81CA6C411C352D91BD65FE0640972E321189D8B11A7CAAF5E1946F2E`.

Aceste valori sunt o observație locală, NU dovada că sursa AUX importată generează același binar. Marchează `installedAuxSourceMatch=UNPROVEN`. Nu relua o investigație nelimitată de proveniență și nu declara potrivire doar din Hardware ID/versiune. Construim un candidat nou, cu sursă și hashuri proprii; înlocuirea driverului instalat va necesita verificare și aprobare separate.

Candidatul C1 existent, numai pentru referință/reutilizare:
`D:\Projects\Entropy-ClockFollow-C1-20261015\candidate\package-retry\ClockFollow-C1-0.3.1.0`.
SYS C1: `F7703F8A004535031EF9F96795E0AEDF1779F75EC1050EF8E48F808ACA6A6764`.
INF C1: `5834F5F6BB05741D341000F74B69EF919FDDD4CABF32D8F552B4F25D4ABC5370`.
Model C1: `03744051BE92919C3365100A80C9876870461AAE7528AF139FBA538BEE8C9813`.
SYS este NotSigned. Nu îl resemna, reîmpacheta sau instala ca efect secundar al C2.

## 3. Un singur workspace nou, nu alte copii istorice

Repository existent pentru fetch/worktree:
`D:\Projects\Entropy-Stage3-Recovery-20261013\live-v1-source`.

Workspace C2 propus:
`D:\Projects\Entropy-ClockFollow-C2-20261015\source`.

Folosește explicit `git -C <cale>` și verifică worktree/repository/HEAD; nu depinde de directorul implicit al terminalului. Creează o singură dată worktree-ul ramurii C2 din commitul exact de instrucțiuni primit. Dacă există deja, validează-l și continuă fără clonare suplimentară. Nu comuta checkouturile C1, B1 sau LiveV1.1. Dacă D: nu este accesibil, cere accesul necesar sau raportează problema exactă; nu substitui automat copia F: și nu recrea `.git`.

Acest task autorizează IMPLEMENTARE + BUILD în C2, spre deosebire de taskurile C1/LiveV1.1 care autorizau numai compilare. Nu aplica instrucțiunile istorice «nu modifica sursa» astfel încât să blocheze implementarea cerută aici. Restricțiile asupra instalării, audio și sistemului rămân ferme.

## 4. Arhitectura de implementat

DATE:
MUSIC virtual -> SRC MUSIC Stage1 ----+
                                    +-> mix Stage1 -> Stage2 -> DSP Stage3 -> PCM24 -> DAC master
AUX virtual   -> SRC AUX Stage1 ------+

CONTROL, separat de PCM:
DAC master -> observații Stage3 -> ClockBridge -> actuatoare MUSIC/AUX.

Organizează explicit modulele: contracte de clock, publisher Stage3, estimator/controller, client driver MUSIC, client driver AUX, adaptor scheduling Stage1 și domeniu direct Stage3. Nu introduce singletonuri de PCM sau acces la obiectele interne ale altui proces.

Un singur estimator al masterului; fiecare actuator are propria sesiune/generație. Eventualele corecții de umplere trebuie să aibă rol și semn documentate și să urmărească datele reale, nu să formeze două bucle concurente care controlează același dezechilibru. În DIRECT_PRIMARY, controllerul WDL al acelui DAC NU rulează. Bridge-ul nu citește PCM și nu ocupă dreptul unic al providerului RAW.

## 5. Integrarea AUX — pe traseul folosit efectiv

Punct important verificat în cod: `Integration/Music/Stage1-v0003-r9/Providers/Legacy/LegacyProvider.cpp`, ramura `LEGACY_FIXED_AUX`, încarcă modulul AUX și apelează `read_source_f64`. Calea este private bridge, nu automat captura WASAPI asociată dispozitivului. Perechea MMDevice render/capture nu autorizează înlocuirea providerului cu un loopback sau deschiderea unui al doilea cititor.

Folosește `Shared/EndpointBridge/src/` pentru a urmări efectiv conectarea KS. Nu schimba formatul numeric Q31/FP64 ori scalele existente pentru a introduce controlul temporal. Nu adăuga SRC în driver.

Portarea actuatorului trebuie să acopere aceeași cronologie care conduce consumul render, poziția, packet count și notificările. În sursa AUX inspectată, `TimerNotifyRT` folosește `m_hnsNotificationInterval` și `UpdatePosition(qpc, entropyExactPacketBytes)`; `UpdatePosition` are separat calea QPC și calea de pachet exact. Un simplu multiplicator adăugat doar în formula de bytes nu controlează neapărat calea de notificare activă. Implementează schimbarea coerent, nu doar o variabilă `ppm` neutilizată.

Reutilizează principiile și aritmetica portabilă C1: acumulare fracționară, schimbarea pantei numai pentru viitor, pauză/reset/expiry explicite. Nu schimba retroactiv poziția și nu tratezi un wake întârziat drept permisiune de a reconsuma DMA nevalid sau a repeta un pachet. Windows rămâne autoritatea asupra datelor scrise/angajate; contabilizează complet cadrele admise.

Adaugă control versionat separat de READ/DIAGNOSTICS, păstrând interfața veche compatibilă. Preferă transport de control separat, cu ownership legat de handle și validare de acces; nu importa automat IOCTL-urile unui driver diferit în dispatchul PortCls. Adaptorul AUX poate diferi de cel MUSIC, dar semantica este comună: capabilities/status/acquire/update/release, stream generation, master epoch, sequence, termen de valabilitate și unități explicite. Nu presupune că identitatea unei sesiuni echivalează cu autentificarea procesului. Documentează limita ACL-ului moștenit din C1; nu relaxa accesul pentru a face probele să treacă.

Păstrează identitățile și funcția endpointurilor AUX pentru viitoarea actualizare controlată. Candidatul AUX are versiune nouă, consecventă în INF/SYS/status, distinctă de 1.1.1.5; nu falsifica aceeași versiune. Nu modifica acum dispozitivul default sau registrul. Nu folosi captura AUX neconsumată ca master separat și nu conecta la ea probe audio.

## 6. Metadate și Stage1

Providerul AUX vechi marchează metadatele ca parțiale și atribuie QPC la citire; acel QPC nu trebuie reinterpretat drept timestampul hardware/originea exactă a cadrului. Adaugă o extensie versionată care livrează atomic, din același snapshot al pachetului, generație, cadru/seq real, domeniu/master epoch și timestamp cu unități. Nicio citire separată care poate asocia datele unei generații cu clock-ul alteia. Capabilitatea trebuie să fie verificată, nu inventată pentru driverul vechi. Păstrează calea `legacy.aux` funcțională în COMPAT.

În Stage1 modifică transportul, nu implementarea r8brain și nici funcția numerică a matricei. SRC-urile MUSIC/AUX rămân instanțe separate la raport nominal, cu memoria/întârzierea lor. Publicarea nu mai impune un ritm mediu rigid `QPC * 192000` independent de master atunci când ClockFollow este selectat.

Stabilește cronologia comună de ieșire, cu conversia rațională a ratelor 44.1/48/... către 192 kHz, resturi păstrate, originile streamurilor și latențele SRC declarate. Ratele virtuale efective urmăresc nominalRate * masterRate/192000. Delimitarea blocurilor nu este timpul muzicii. Nu goli/reumple SRC sau FIFO la fiecare corecție și nu schimba ordinea cadrelor pentru a alinia mixul.

MUSIC singur, AUX singur și mixul lor trebuie să funcționeze. Sursa cu adevărat inactivă contribuie cu tăcere declarată și nu blochează cealaltă sursă. Nu confunda idle cu RUN fără date. Nu declara blocuri sintetice valide pentru a satisface timerul; un underrun real al unei contribuții active trebuie semnalat conform politicii explicite de așteptare/fault.

Extinde numai adaptoarele IPC/metadatele unde contractul actual nu transportă informațiile necesare. Algoritmii Stage2, extractorul de centru, EQ, loudness și latențele lor NU se modifică. Nu da Stage2 dreptul de a regla clock-ul. Dacă trebuie propagată metadată prin adaptorul Stage2, izolează schimbarea de DSP și dovedește păstrarea mostrelor/countului. Respingerea versiunilor incompatibile este explicită; nu eticheta ieșirea unui Stage2 necunoscut ca sincronizată prin presupunere.

## 7. Observațiile masterului și ClockBridge

Masterul este UN endpoint fizic DAC selectat explicit în configurația Stage3, identificat prin endpoint ID și generația streamului. ID-urile AUX din acest document NU sunt DAC-ul master. Nu alege după prima enumerare, eticheta A/B sau dispozitivul default Windows. Configurația nouă poate rămâne fără master la build; opțiunea directă rămâne indisponibilă până la selecție validă.

Observațiile provin de la clientul WASAPI pe care îl deține deja sink-ul. Bridge-ul nu deschide o a doua ieșire. Publică snapshoturi coerente, prealocate, fără IOCTL, așteptări, alocări, JSON sau fișiere în firul care trebuie să livreze bufferul DAC. Un fir de control separat poate expune snapshoturile prin IPC versionat, cu limite și ownership explicite.

Câmpuri necesare: versiune, size, master endpoint identity, master epoch, stream state, sample rate, poziție nativă, frecvența unității de poziție, timestampul corelat de 100 ns, sequence și validitate. Nu combina `position` din apelul nou cu `QPC` din apelul anterior. Nu folosi callback wake time sau numărul octeților trimiși ca substitut pentru observația de clock.

`IAudioClock::GetPosition` și `GetFrequency` se interpretează împreună. QPC-ul din GetPosition este în unități de 100 ns; QPC-ul privat C1 este în ticks cu frecvență explicită. Convertește fără overflow/pierderi acumulate și respinge observațiile nevalide, înapoi în timp, din alt epoch sau insuficient de precise (`S_FALSE` nu este aceeași calificare ca `S_OK`).

ClockBridge rulează separat de PCM, cu un singur proprietar și heartbeat. Pentru MUSIC folosește exact ABI-ul C1 din `protocol/ClockBridgeV1.h`: Request 56 bytes, Reply 128 bytes, corecție în ppb, nu ppm. Negociază capabilities. .11 fără actuator nu primește comenzi de control și nu poate primi eticheta FOLLOWING.

Păstrează limitele C1: ±1000 ppm, 20 ppm/s limită a variației aplicate la update și lease 2 s, dacă nu există motiv tehnic separat și aprobat să se schimbe ABI-ul. Retransmiterea unei observații vechi nu trebuie să mențină fals sincronizarea. Bridge-ul urmărește feedbackul efectiv aplicat, nu presupune că requested=applied.

Estimatorul urmărește rata pe o fereastră justificată și tratează separat jitterul de livrare de deriva susținută. Pune coeficienții într-un singur modul/policy și verifică stabilitatea sintetic, nu prin ore de încercări pe muzică. Semnul corecției este: DAC mai rapid -> producție virtuală mai rapidă; exces persistent de date în transport nu trebuie agravat prin creșterea producției. Definește explicit soldul urmărit, inclusiv bufferele relevante, fără dublă numărare și fără «mascarea» lui într-un rezervor intermediar.

## 8. Stage3 DIRECT_PRIMARY și lifecycle

Introdu un domeniu direct separat de domeniul WDL: preia cadrele FP64 în ordine, aplică originalul PCM24 și livrează prin aceeași separare preparation worker / ready packets / sink owner. Nu este suficient `SetRates(192000,192000)`: în DIRECT_PRIMARY nu apelezi interpolatorul, filtrul sinc sau controllerul WDL pentru master. Toate canalele masterului folosesc aceeași cronologie și același contract de cadre.

Păstrează DSP Stage3, matrice, gain și modul Headroom al profilului. Fără AUTO introdus, rezervă de gain, limiter sau EQ nou. Compararea bit-transparentă privește ieșirea DSP către intrarea cuantizării, nu identitatea dintre intrarea Stage3 și difuzoare.

Definește stări separate: COMPAT_ASYNC, ACQUIRING, DIRECT_PRIMARY, HOLD/FAULT, DISARMED. Nu confunda «are lease» cu «rata este stabilă» ori «mostrele tuturor surselor din mix sunt în domeniul master».

Pornirea nu trebuie să fie circulară. Driverul funcționează autonom înaintea DAC-ului; clock-ul hardware poate fi observat numai după Start. Proiectează explicit pre-roll-ul și momentul de admitere a cronologiei audio. Un pre-roll SILENT declarat înaintea admiterii audio este permis, dar nu pretinde că el păstrează audio produs înainte de ARM. Nu acumula date nelimitat în timpul achiziției și nu elimina date dintr-un stream deja admis ca să recuperezi poziția. Nu activa ASRC și ClockFollow ca două bucle care corectează simultan aceeași sursă în timpul achiziției.

La pierderea masterului/lease-ului, ieșirea directă ajunge controlat în HOLD/FAULT înainte ca bilanțul să fie încălcat. Nu trece pe WDL ori pe alt DAC pe ascuns la mijlocul streamului. Schimbarea modului/masterului se face la graniță de control cu oprire/pre-roll și epoch nou. UI și procesele rămân disponibile; DISARM trebuie să anuleze prompt așteptările.

MUSIC/CINEMA păstrează corecția LiveV1.1: selectarea nu cere publicarea simultană a ambelor surse. CINEMA fără date rămâne selectat în așteptare. În CINEMA, folosește calea compatibilă asincronă existentă, nu DIRECT_PRIMARY numai pentru că formatul este FP64/192.

UR23 primește flux optic extern. Nu atinge biții transportului DD/DTS, decodorul sau SRC-ul lui în C2. Nu pretinde controlul clock-ului UR23 prin driverul virtual MUSIC. Adaptarea CINEMA la o singură conversie după decodare rămâne o intervenție ulterioară, nu un motiv de a modifica acum două SRC-uri deodată.

DAC-ul secundar rămâne independent, cu propriul ASRC și fault local. Nu folosi starea lui pentru a bloca principalul. Alinierea fizică precisă între două DAC-uri nu este calificată de acest task și nu trebuie raportată ca PASS.

Păstrează layoutul UI existent. Expune modul, masterul, starea de urmărire și motivul indisponibilității în zona de dispozitive/control, fără o redesenare generală. Numai controlul minim necesar poate fi adăugat. Nu activa ARM și nu modifica traseele difuzoarelor ca să demonstrezi funcția.

## 9. Verificări necesare în implementare, nu o campanie audio

Folosește teste locale punctuale, deterministe, pentru schimbările tale. Nu rula automat toate suitele istorice, benchmarkuri, Tidal, REW, capturi live sau probe de difuzoare.

Acoperă în grupuri compacte:
1. Unitatea clock/lease: semn, unități, continuitate, fracții, schimbări generation/master, replay/expiry/release și lipsa contendenței cu cititorul PCM.
2. Bindingul AUX real: corecția schimbă cronologia render/notificările/pachetizarea efectiv folosite, nu doar variabila dintr-un model nelegat; PCM/ordinea rămân identice în traseul valid.
3. Transport/mix: MUSIC la 44.1 și AUX la 48 cu același master; fiecare singur, ambele active, idle/start/pause/format change; jitter de pachete separat de rata medie. Verifică bilanțul pe suficiente cadre sintetice pentru a vedea deriva, nu doar câteva callbackuri.
4. DIRECT_PRIMARY: mostre după DSP == mostre înainte de PCM24 pe toate canalele, niciun apel ASRC pe master, pregătire înaintea deadline-ului de submit, fără duplicare/drop la schimbarea mărimii pachetului; compatibilitatea CINEMA/secondary rămâne explicită.
5. Lifecycle: bootstrap fără deadlock, stale master, bridge absent/încetat, DISARM, source wait, mode switch și fault local al secundarului.

Testele sintetice și buildul nu califică kernelul încărcat, driverele fizice sau calitatea audio. Nu raporta trecerea unui model ca dovadă pentru o cale care încă nu îl folosește. Erorile de implementare/compilare introduse de tine pot fi corectate în acest scope; nu trebuie oprit definitiv taskul la prima eroare reparabilă proprie. Oprește și raportează numai când lipsesc accesul/datele necesare, este cerută o schimbare de arhitectură neaprobată sau ar trebui încălcată o limită de instalare/securitate. Nu suprima avertismente/teste ca să obții verde.

## 10. Build și pachet reluabil

Folosește MSVC/IPP/Qt/SDK/WDK deja instalate și dependințele pin-uite. Nu impune AVX2 pe CPU-ul utilizatorului, nu adăuga Faust/Rust și nu schimba politica FP64 sau bibliotecile DSP. Rezolvă în script căile aprobate prin parametri/descoperire explicită; nu modifica global toolchainul.

Construiește componentele C2 modificate: driver AUX candidat, client/bridge, Stage1 candidat, Stage3 candidat și UI/adaptoare numai dacă s-au schimbat. C1 deja construit poate fi referit ca dependență separată prin manifest; nu îl relabela ca binar C2 și nu îl recompilezi inutil. Dacă descoperi un defect C1 care impune modificarea lui, raportează precis înainte de a invalida candidatul existent.

Creează `ClockFollow/Build-C2.ps1` cu mod BuildOnly și PackageOnly care reia din artefacte/loguri verificate. Pachetul nou derivă căile din propriul manifest, nu din folderul activ al Codex. Fișierele INF generate identice pot fi selectate după hash și cale deterministă; copiile diferite sunt eroare. Windows PowerShell 5.1 trebuie suportat, inclusiv fără presupunerea că `Path.GetRelativePath` există.

Destinație propusă: `D:\Projects\Entropy-ClockFollow-C2-20261015\candidate\C2-integration`.
Nu suprascrie pachete și nu șterge loguri incomplete. Artefactele se identifică prin source commit, imported refs, hashuri, versiuni, build results și flags implemented/enabled/deployed. Separați sursa binarului de revizia de împachetare la reluare.

Copiază configurațiile SALVATE ale utilizatorului numai în noul pachet, cu hashuri originale; nu presupune că o captură sau un profil vechi reprezintă reglajele nesalvate. Nu activa direct/ARM în ele. Stage1, Stage2 și Stage3 instalate, AUX default, C1 existent, DLL-urile de referință și profilele originale rămân neatinse.

Verifică la build că noul executabil chiar leagă/rutează implementarea C2, nu doar că există fișierele. Creează launcherul viitor și o verificare de readiness care refuză DIRECT_PRIMARY când driverele cu capabilities necesare nu sunt instalate. Nu executa această verificare cu acces live la dispozitive în taskul de build. Un launcher construit nu înseamnă că instalarea este deja aprobată.

NU semna, nu importa/folosi chei sau certificate, nu genera un catalog prezentat drept instalat, nu modifica PnP/INF/registry/boot/Secure Boot/test-signing/servicii/default audio. Nu porni aplicațiile audio, nu trimite ARM și nu opri procesele utilizatorului. Pregătirea pachetului și planului de deployment nu autorizează execuția deploymentului.

## 11. Încheierea taskului

La final, returnează: rezumatul schimbărilor reale; componentele integrate; rezultatul buildurilor/verificărilor punctuale; hashurile; calea pachetului; limitările; pașii exacți care vor necesita separat semnare/instalare/validare live.

Fă commituri locale coerente în ramura C2; nu schimba main, referințele ori ramurile anterioare. Nu include chei, fișiere personale, capturi, cache/SDK/WDK sau binare de instalare în commit. Nu face push/merge fără cererea utilizatorului. Pregătește diff-ul și SHA-ul pentru revizuire.

Dacă rezultatul este numai parțial, numește exact partea lipsă. Nu declara C2 complet pentru că s-a compilat un helper sau pentru că ruta compatibilă veche pornește. Nu cere iar identificarea AUX din secțiunea 2.

## Referințe tehnice pentru implementator

Citite/verificate pentru acest task:
- `Integration/Music/Stage1-v0003-r9/Providers/Legacy/LegacyProvider.cpp` la baza aplicațiilor: private AUX read, metadata partial și QPC la citire.
- `Integration/Music/Stage1-v0003-r9/Engine/Core/EngineLifecycle.cpp`: scheduler și publicare; `Engine/SRC/SourceDomain.*`: SRC și cozi.
- `Drivers/EntropyMusicRenderV2/protocol/ClockBridgeV1.h` și `CLOCKFOLLOW-C1.md` la ref C1: ABI, limite, lease și limite de securitate/validare.
- AUX `EndpointsCommon/minwavertstream.cpp`: `UpdatePosition`, `TimerNotifyRT`, packet-exact mode.
- Microsoft IAudioClock::GetPosition: `https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudioclock-getposition`.
- Microsoft Audio Property Requests: `https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-property-requests`.
- Microsoft Defining I/O Control Codes: `https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/defining-i-o-control-codes`.

Acest document stabilește cerințe/design. Nu presupune existența unor fișiere sau funcții C2 descrise ca rezultate de creat.
