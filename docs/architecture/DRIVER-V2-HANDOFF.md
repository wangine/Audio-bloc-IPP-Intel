# Entropy Music Render V2 — driverul independent nr. 3

Data: 2026-10-11. Stare: direcție și specificație aprobate de utilizator;
planul se execută direct în acest chat. Kernel0.3.0.10 este semnat DEV și instalat
cu aprobarea explicită a utilizatorului. Cele12 formate Exclusive Event, proba
de30minute și PAUSE/RUN trec comparația PCM integrală; Polling rămâne refuzat.
Probele native192k au identificat goluri în publicația FP64 a Stage1v0002, cu AUX
dezactivat. Core rămâne nemodificat, iar calificarea completă a lanțului nu este
încheiată. Vezi [rezultatele și limitele](../../validation/music-render-v2/LOADED-DRIVER-0.3.0.10.md)
și [pachetul permanent](../../Integration/Drivers/EntropyMusicRenderV2/0.3.0.10-signed-dev-r6).

Acest handoff completează [documentul păstrat în snapshotul Stage1 v0002](../../Integration/Music/Stage1/docs/DRIVER-V2-HANDOFF.md).
Snapshotul, arhivele acceptate și codul Stage1 v0002 rămân nemodificate.
Această păstrare este limita etapei driverului MUSIC, nu o înghețare permanentă
a Core-ului. [Faultul AUX raportat](FOLLOW-UP-STAGE1-AUX.md) va fi investigat după
construcția și validarea driverului; o versiune nouă a Core-ului poate fi modificată
atunci, cu păstrarea v0002 drept referință și protejarea MUSIC stabil.
„Nr. 3” identifică noua implementare, independentă de Transport și Virtual;
„V2” este numele produsului și contractul Provider, nu a treia revizie a Core-ului.

## 1. Scop și limite

Un singur endpoint eRender stereo, `Entropy Music Render V2`, implementat cu
WaveRT/PortCls, selectabil prin WASAPI Shared/Exclusive de TIDAL, Spotify și
clientul de test. Driverul transportă exact octeții primiți de la Windows.
Provider V2 livrează acest PCM prin ABI-ul existent. Numai Stage1 convertește
PCM în FP64 și folosește r8brain pentru ieșirea stereo la 192 kHz.

```text
Aplicație → WASAPI → WaveRT/PortCls → snapshot PCM valid + frame index
         → ring privat bounded → Provider V2 raw → Stage1 v0002
         → PCM-to-FP64 → SRC MUSIC → L/R FP64/192 → Stage2 v0009 → L/R/C → Stage3
```

În Shared, „original” înseamnă bytes primiți de driver după motorul audio Windows.
Nu înseamnă automat bytes ai fișierului muzical sau ai aplicației înainte de mix.
În Exclusive, identitatea față de PCM-ul unui client WASAPI controlat poate fi
verificată de la primul la ultimul eșantion.

MUSIC și AUX continuă să aibă domenii SRC/FIFO separate în Stage1. Noul driver
deservește MUSIC; nu introduce AUX, capture, loopback public, Cinema sau DAC real.
Driverul și Provider-ul nu introduc Q31, FP64, SRC, gain, mute, dither, limiter,
APO de procesare, Qt, client Direct KS, ASIO sau transport MME propriu.
KS intern este infrastructura WaveRT, nu o interfață alternativă pentru aplicații.

## 2. Bază Microsoft și separarea proiectelor

Referință aleasă: Microsoft Windows-driver-samples,
commit `2dc3fd3a0cc84a2933f2194e7ec0871584979071`, directorul
[`audio/simpleaudiosample`](https://github.com/microsoft/Windows-driver-samples/tree/2dc3fd3a0cc84a2933f2194e7ec0871584979071/audio/simpleaudiosample).
Licența repository-ului este MS-PL; importul ulterior trebuie să păstreze licența,
notificările individuale și inventarul exact al fișierelor importate.
Pin-ul a fost importat cu47 blob hashes verificate și compilat separat ca referință.
Fișierele examinate și Git blob hashes sunt în [driver-v2-reference.json](driver-v2-reference.json).

Implementarea va avea trei componente separate: driver kernel, DLL Provider V2
user-mode și utilitar de audit WASAPI/PCM. Ele vor avea surse și build-uri proprii,
fără a introduce dependențe kernel/Qt în Core v0002. Identitatea PnP, INF,
interface GUID, serviciul și providerId vor fi noi; nu vor reutiliza dispozitivele
sau interfețele celor două drivere existente.

Sample-ul include speaker și microphone array, generare de ton și salvare audio.
Pentru noul endpoint rămâne numai render stereo; acele facilități demonstrative
nu fac parte din produs. Codul de ceas, commit și EOS trebuie implementat și
testat conform contractului de aici, nu acceptat automat din sample.

Pe această mașină există WDK/SDK 10.0.28000.0, `portcls.h`, `portcls.lib` x64 și
fișiere WindowsDriver props. Build-ul x64 folosește SDK/WDK28000.2526 extrase local
din NuGet cu SHA256, plus MSVC19.51. Build-ul kernel și generarea catalogului au
fost verificate; SYS/CAT rămân nesemnate, încărcarea și acceptarea live neexecutate.

## 3. Formate inițiale exacte

| Câmp | PCM16 | PCM24 valid în 32 |
| --- | --- | --- |
| Canale / channelMask | 2 / FL+FR = 3 | 2 / FL+FR = 3 |
| containerBits / validBits | 16 / 16 | 32 / 24, MSB-aligned |
| blockAlign | 4 bytes | 8 bytes |
| Rate | 44100, 48000, 88200, 96000, 176400, 192000 Hz | aceleași șase |
| SubFormat | KSDATAFORMAT_SUBTYPE_PCM | KSDATAFORMAT_SUBTYPE_PCM |

Validarea verifică lungimile structurilor, tag-ul, SubFormat GUID, canalele,
mask-ul, rata, containerul, valid bits, blockAlign și averageBytesPerSecond.
PCM16 WAVEFORMATEX poate fi reprezentat canonic pentru ABI cu GUID PCM și mask3;
PCM24/container32 necesită descriere extensible care exprimă validBits=24.
Orice canonicalizare de metadate trebuie documentată; payload-ul nu se schimbă.
Nu se anunță și nu se acceptă Float32, PCM32 valid32 sau packed24 în prima etapă.
Negocierea trebuie să refuze formatul nesuportat, fără fallback intern.
În Shared, conversia făcută de Windows înainte de driver se înregistrează separat.

## 4. Un singur model de ceas

Pentru fiecare generație de stream se păstrează QPC frequency reală, totalul
tick-urilor petrecute în RUN, rata negociată și un contor logic uint64 de cadre.

```text
N(t) = floor(totalRunningQpcTicks(t) * sampleRate / qpcFrequency)
linearBytes = N(t) * blockAlign
DMA offset = linearBytes modulo bufferBytes
```

Calculul folosește aritmetică întreagă verificată la overflow, cu cât/remainder
sau operații wide echivalente. Nu se acumulează deplasări rotunjite la milisecunde.
PAUSE îngheață avansul; RUN continuă de la aceeași fracție și poziție, fără a
include durata pauzei. STOP încheie generația și resetează pozițiile KS și
packet count. Un RUN după STOP folosește o generație nouă, nenulă.

Play/linear/presentation positions, write lead-ul declarat și notificările provin
din același model. Offseturile sunt aliniate pe cadre. Timerul doar trezește
driverul; numărul de callback-uri nu este ceasul și nu este packet count.
Packet count KS se extinde intern dincolo de ULONG; numai la DDI se expune
partea cerută de Windows. Wrap-ul ULONG și wrap-ul DMA sunt evenimente diferite.

## 5. Trei cronologii distincte: scris, transferat, livrat

Se păstrează separat:

- Pachetele declarate scrise/valide de Windows și snapshoturile lor imuabile.
- Cadrele și pachetele transferate la timpul virtual de render.
- Pachetele PCM publicate/citite prin bridge, plus pierderile și anulările.

[`SetWritePacket`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertoutputstream-setwritepacket)
anunță date valide; nu declară transferul deja încheiat.
[`GetPacketCount`](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertoutputstream-getpacketcount)
raportează pachete complet transferate și se resetează în STOP.
Notificările și avansul nominal continuă conform contractului Windows și la
underrun; bridge-ul nu inventează PCM valid pentru a umple acel interval.

În packet/notification mode, validarea commit-ului și copierea exactă în memorie
privată au loc înainte ca Windows să poată reutiliza slotul WaveRT. Ownerul
snapshoturilor există când streamul/bufferul sunt pregătite, nu abia în RUN.
Prefill-ul acceptat înainte de RUN se păstrează: primul packet are firstSourceFrame0,
nu se șterge la RUN și nu se publică de două ori. Provider-ul primește datele
după activarea RUN și maturizarea transferului corespunzător, nu anticipat doar
pentru că următorul pachet a fost scris în avans.

Un singur owner serializează poziția, eligibilitatea pentru livrare și commit-ul
ringului. SetWritePacket poate crea snapshotul; UpdatePosition poate marca
transferul lui, dar nu recopiază și nu publică același payload ca al doilea producer.
Query-urile de poziție sunt idempotente față de publicare. Numerele OS de pachet
se validează cu aritmetică modulo, într-o fereastră limitată de buffer; pachetele
late, duplicate sau prea avansate nu sunt acceptate ca date noi.

## 6. Polling și limitele de ownership

Modul de servicing al clientului WASAPI nu se confundă automat cu modul de
buffer WaveRT. Utilitarul trebuie să înregistreze allocation/notification mode-ul
real și apelurile efective, separat de flag-ul WASAPI EVENTCALLBACK.

În sample-ul pin-uit, SetWritePacket/GetPacketCount returnează NOT_SUPPORTED
pentru buffer fără notificări. Așadar, suportul polling nu se poate declara
implementat doar fiindcă există SetWritePacket.

Pentru un buffer position-driven se alege explicit un singur owner de consum:
snapshot al intervalului efectiv consumat, aliniat pe cadre, înainte de expunerea
reutilizării sale către OS, cu wrap și write lead coerente. Nu se comută ownerul
după „primul SetWritePacket observat” și nu se publică simultan pe ambele căi.
Avansul QPC, singur, nu dovedește că o regiune a fost deja scrisă valid de client.
Proba controlată trebuie să demonstreze ownership-ul și să excludă citirea unei
regiuni încă în curs de scriere, inclusiv sub întârziere și la start/wrap.
Dacă acest invariant nu poate fi demonstrat pentru calea reală, acea cale rămâne
neacceptată; nu este compensată prin gain, mute, latență ascunsă sau contoare false.

## 7. EOS, STOP/RUN și capetele secvenței

Acceptarea PCM compară o secvență finită completă, inclusiv primul și ultimul cadru.
Clientul de test trebuie să lase ultima regiune să fie transferată și citită înainte
de STOP. EOS/ultimul packet parțial sunt validate și tăiate numai la lungimea
validă declarată, aliniată pe cadre; bytes de padding nu devin eșantioane noi.
EOS cu lungime zero este tratat ca stare de control, nu ca packet PCM cu zero
cadre trimis către decoderul Stage1.

Un STOP explicit care anulează coada încă netransferată este raportat ca anulare,
nu ascuns ca PASS pentru o probă care pretinde drain complet. Restartul golește
datele vechii generații. PAUSE/RUN păstrează generația și prefill-ul valid.
Schimbarea formatului necesită renegociere și generație nouă; niciun packet nu
conține metadate din formatul nou cu payload din formatul vechi.

## 8. Ring și Provider V2

Bridge-ul privat are protocol versionat, dimensiuni explicite, fără pointeri
kernel în wire data. Un consumator deține sesiunea; un al doilea este refuzat.
Payload + descriptor sunt publicate atomic numai după copierea completă.
Memoria este bounded, alocată la prepare, fără alocări sau filesystem în calea audio.
Configurația inițială propusă: 262144 cadre pentru payload-ul ringului, 4096
descriptori și maxim 65536 cadre per packet; snapshoturile pending au spațiu
separat, limitat de bufferul WaveRT acceptat. La PCM24/container32, payload-ul
ringului are 2 MiB. Limita de bytes și cea de descriptori se verifică independent.
Capacitatea este retenție maximă, nu o latență țintă sau prebuffer obligatoriu.
Capacitatea, maximul de packet și write lead-ul sunt expuse în diagnostice și
validate la negociere; nu se acceptă mai mult decât poate stoca ringul.

Absența Stage1/Qt/Stage2 nu oprește ceasul driverului. Un consumator absent sau
prea lent poate depăși orice ring finit: overflow-ul refuză noile livrări fără
suprascrierea payload-ului citit și contabilizează exact cadrele nelivrate.
Următorul descriptor valid păstrează indexul real, lostFrames și discontinuity.
lostFrames include cadrele valide committed, dar nelivrate; underrun-ul pentru
date nescrise de OS are contor separat. Se păstrează și totalurile pe durata
driverului și raportul generației încheiate, astfel încât STOP/restart să nu
șteargă dovada. Cititorul se conectează înainte de start pentru proba zero-loss;
un cititor conectat târziu nu este prezentat drept captură de la cadrul0.
Ținta zero pierderi se acceptă numai în condițiile și intervalele măsurate cu
consumator activ; nu se promite retenție nelimitată fără consumator.

DLL-ul nou implementează [endpoint_provider_v2.h](../../Integration/Music/Stage1/ABI/endpoint_provider_v2.h)
fără modificarea ABI sau Core. Mapping-ul obligatoriu:

| ABI | Semnificație |
| --- | --- |
| capabilities / representation | ENTROPY_CAP_RAW_PCM / ENTROPY_REP_PCM |
| flags | RAW_IDENTITY pentru cronologie verificată; DISCONTINUITY la gol real |
| generation | Identitate nenulă a streamului, stabilă la PAUSE/RUN |
| packetSequence | uint64 nenul, monoton; discontinuitățile nu sunt renumerotate |
| firstSourceFrame / frames | Indexul real al intervalului și lungimea exactă |
| format / SubFormat | Formatul complet negociat, GUID în reprezentarea Windows |
| payloadBytes | frames × blockAlign, octeți interleaved L/R copiați integral |
| originalContainerBits / originalValidBits | egale cu containerBits / validBits pentru PCM raw |
| qpcTimestamp / qpcFrequency | QPC real al snapshotului/commit-ului; frecvența reală |
| lostFrames | cumulativ în generația curentă, fără reset care ascunde pierderi |
| reserved | zero, fără extensii ad-hoc ale ABI |

packetSequence Provider este distinct de ULONG PacketNumber OS; separarea sau
regruparea într-un packet ABI este documentată și păstrează firstSourceFrame.
Nicio livrare nu depășește ENTROPY_MAX_PACKET_FRAMES=65536.
`read` returnează 1 numai pentru un packet complet, 0 când nu există date și
eroare la protocol invalid; `cancel` întrerupe așteptarea, iar `close` eliberează
ownership-ul fără acces ulterior la memorie eliberată.
IOCTL-urile verifică versiunea, toate lungimile și aritmetica bytes/cadre înainte
de acces. Nu expun pointeri sau memorie kernel arbitrară. Cititorul vede numai
sloturi committed; cancel/close/free-buffer folosesc ownership și rundown explicite.
Un buffer user insuficient nu consumă și nu fragmentează implicit packet-ul.

Parametrul de access al `open` și modul WASAPI al aplicației sunt lucruri diferite.
Shared/Exclusive în packet.mode se raportează numai dacă există o sursă verificată
a informației. Altfel rămâne UNKNOWN; nu se deduce din PCM bits, perioada bufferului
sau alegerea de access din UI. Harness-ul înregistrează separat modul cu care a
apelat IAudioClient::Initialize. Pentru TIDAL se înregistrează metoda prin care
modul a fost observat și nivelul de certitudine.

## 9. Exact trei grupe de acceptare

### A. WASAPI Exclusive → ABI Stage1: PCM integral

12 formate × Event-Driven/Polling = 24 cazuri de bază pentru noul endpoint.
Semnale PCM cunoscute, L/R distincte, incluzând zero, extreme și secvențe care
identifică poziția fiecărui cadru. Se compară fiecare byte al containerului înainte
de decode/SRC, nu numai valoarea FP64 sau conversia Q31.
PASS cere frame0, ultimul cadru, număr total exact, zero mismatch/loss/duplicate.
Niciun prefix de „warm-up” nu se elimină din comparație.

Auditul folosește același Provider/ABI pe care îl încarcă Stage1. În testul cu
Core real, observația capturează payload-ul predat efectiv lui prin instrumentarea
de test a Provider-ului, nu un al doilea cititor care fură pachete din bridge.
Captura de audit are propriul contor de overflow; o captură incompletă nu poate
certifica integritatea. Core v0002 și DSP-ul său rămân aceleași.

### B. Clock și continuitate

Teste deterministe pentru QPC, fracții, pauză, STOP/RUN, renegociere, generații,
prefill și EOS; wrap DMA, wrap ULONG packet number, timeout/cancel și overflow.
Simularea wrap-ului nu este prezentată ca test kernel live.
Teste reale cu query-uri frecvente, întârzieri ale cititorului în capacitatea
declarată, packet sizes acceptate și rulare continuă; pentru fiecare se păstrează
durata, cadrele, fill maxim, latența de servicing și contractul efectiv.
PASS cere poziții coerente, fără citire prematură, duplicare sau pierdere în
intervalul acceptat. Overload-ul injectat trebuie detectat exact și raportat ca
atare; nu este un caz zero-loss.

### C. TIDAL real: diagnostic stereo înainte de SRC

Se măsoară L/R peak/RMS, Mid/Side RMS și corelația pe ferestre comune, separat
Shared/Exclusive. Raportul păstrează formatul efectiv, generația, cadrele,
modul observat, volumele/configurația relevante și intervalele de tranziție.
Se utilizează metrica pre-SRC existentă în SourceDomain, fără alt procesor audio.
Raportul M/S este exprimat în dB cu tratament explicit pentru RMS zero.

Core v0002 nu expune Pearson explicit. Din RMS-urile existente ale aceleiași
ferestre se poate calcula corelația necentrată:

```text
E[L*R] = RMS(M)^2 - RMS(S)^2
rho_uncentered = E[L*R] / (RMS(L) * RMS(R))
```

Aceasta nu este Pearson dacă există DC. Utilitarul de audit poate calcula Pearson
din PCM-ul brut, cu medii L/R și covarianță, în afara driverului/Core DSP.
UI snapshots ale unor blocuri diferite nu se agregă ca o fereastră comună;
ferestrele de audit sunt identificate prin firstSourceFrame și număr de cadre.
Stereo păstrat nu înseamnă automat RMS pre/post SRC identic: SRC-ul schimbă
grila temporală și are tranzient de pornire. Comparațiile țin cont de acestea.

A+B validează numai modurile, formatele și condițiile testate. C nu poate dovedi
că TIDAL furnizează același PCM intern în Shared și Exclusive.

## 10. Dovezi existente și ce nu demonstrează

[Auditul vechilor drivere](DRIVER-LIVE-AUDIT-20261011.md) consemnează 48 cazuri și
15.453.202 cadre stereo comparate după tratamentul intervalului inițial.
Exclusive Event-Driven pierde primul packet în 24/24 cazuri, pe ambele dispozitive;
polling păstrează cadrul0, iar secvențele ulterioare comparate nu au coruperi.
Proba veche compara reprezentarea Q31, nu transportul raw propus aici, și nu
califica EOS/drain complet sau stabilitate de ordinul orelor.

Sursa locală Music Transport confirmă că Submit respinge un owner inactiv înainte
de RUN. Pentru Virtual, observația confirmă aceeași lipsă; nu certifică identitatea
sursei kernel față de build-ul instalat. Aceste rezultate nu atribuie automat
o pierdere continuă de dinamică defectului de start.

LegacyProvider livrează F64_PLANAR cu METADATA_PARTIAL și packet.mode UNKNOWN,
fără RAW_IDENTITY. Este o referință de compatibilitate, nu dovadă a identității
PCM originale. Noul Provider raw trebuie să înlocuiască acea limită pentru
testele driverului, păstrând legacy disponibil separat.

## 11. Ordinea implementării după revizia specificației

Actualizare11octombrie2026: kernel0.3.0.11 și Stage1v0003-r3 sunt livrate separat,
cu toolul raw/FP64 păstrat. [Raportul încărcat](../../validation/music-render-v2/LOADED-DRIVER-0.3.0.11.md)
consemnează corectarea așteptării pasive rare de16ms, cele12formate raw,
23.040.000cadre native FP64 cu drain complet și testele AUX. Referința v0002,
rezultatele0.3.0.10/r6 și limitările TIDAL/polling/EOS rămân distincte.

[Planul concret de implementare](../superpowers/plans/2026-10-11-music-render-v2-driver.md)
păstrează execuția direct în acest chat, conform preferinței utilizatorului.

1. Plan concret și inventar de import/licențe pentru pin-ul Microsoft; build kernel x64 verificat.
2. Model comun de clock/ownership/ring, cu teste deterministe de start și wrap înainte de integrare.
3. Endpoint render stereo și negociere strictă; commit notification/polling, EOS și lifecycle verificate.
4. Provider raw și harness WASAPI pentru acceptarea A+B; Core v0002 rămâne referință.
5. Pachet INF/SYS/CAT verificabil și raport de build/signing, apoi instalare autorizată explicit.
6. A+B pe driverul încărcat, apoi măsurarea C cu TIDAL și lanțul stabil Stage1/2/3.

Rezultatele încărcate sunt consemnate separat; această specificație nu înlocuiește
probe reale. Instalarea și semnarea au fost autorizate explicit. Nu s-au schimbat
certificatele sau politica de boot și nicio instalare nu este efect secundar de build.
