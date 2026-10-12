# Stage1 MUSIC/AUX — Faza A, inventar și contract

Data: 2026-10-10. Referință de cerințe: `instruction(20261010-180432).md`, aprobată pentru continuare de utilizator. Această fază documentează extragerea; nu pornește capture, Qt, Stage2 sau drivere.

## Referința verificată

- Repository: `wangine/Audio-bloc-IPP-Intel`.
- Tag remote `stage1-passive-v0002`: commit `48f979f489264885ab1a16351f81c43ddf64b0f1`, verificat prin `git ls-remote`. Obiectul commit există local, deși numele tagului nu există în refs locale.
- Checkout-ul local rămâne `codex/pcm24-v2-integration`, HEAD `3e36e36`; nu s-a schimbat checkout-ul. Fișierele se citesc din commit, nu se confundă cu HEAD.
- `git diff --name-only <commit> v0.1.9 -- Integration/Stage1/native-src` este gol: sursele native SRC coincid.
- Freeze: `STAGE1-PHASE-A-FREEZE.json`, 351 obiecte Git pentru Stage1 și transportul pasiv, plus SHA256 pentru document, arhiva 0.1.9 și referința Matrix locală.
- Arhiva 0.1.9: SHA256 `DAD485108CB12273F48104294B4AF688099A356E45AEEAA55AC87AE99A5957FC`.

## Componente și tratamentul lor

| Componentă citită | Observație în cod | Utilizare în Stage1 nou |
|---|---|---|
| `Integration/Stage1/native-src/Modules/SRC/src/MultiChannelSrcEngine.cpp` și header | r8brain FP64, 2.0% transition band, 217 dB, linear phase; same-rate copy; prepare separat | Copie verificată și link static MSVC; două instanțe stereo independente. Nu se rescrie algoritmul. |
| `Modules/SRC/R8Brain/src/r8brain-free-src` | Implementare și licență prezente | Copie cu proveniență și licență; fără benchmarkuri importate în build. |
| `Integration/Stage1/source/Host/EntropyQtHost/Core/ProgramMixer.cpp/.h` | `L = MUSIC_L + AUX_L`, `R = MUSIC_R + AUX_R`; overload doar telemetrie | Reutilizare nativă; fără normalizare/limiter. |
| Matrix local: `D:/Projects/Entropy Codex Lucru/Modular 3 stages/source/Modules/InputMatrix/src/InputMatrixEngine.cpp` | MUSIC și AUX sunt rutate separat înainte de ProgramMixer; mute raised cosine de 5 ms. Codul are și CINEMA | Extragere doar a rutelor MUSIC/AUX și rampelor în `Stage1/Engine/Matrix/InputMatrix`. Hash-urile locale sunt înghețate; această sursă nu este prezentată ca provenind din tag. Comparare cu DLL-ul Matrix al tagului. |
| `SourceFifo192`, `SourceDomainStream`, `SourceReadiness` | FIFO/generații utile, dar lifecycle și module legacy cu trei domenii | Referință pentru ownership; FIFO și lifecycle stereo noi, fără importarea hostului. |
| `Transport/StagePublishV2` din tag | Transport pasiv existent pentru layout-urile vechi | Se copiază varianta MUSIC deja validată din `Stage2/transport/StagePublishV2`, împreună cu SDK-ul și hashes, fără editarea Stage2. |
| Endpoint API/catalog V1 și cele trei DLL-uri de intrare din tag | C ABI, FP64 convertit; status nu include full SubFormat GUID, packet sequence și primul source frame | Adaptoare de referință izolate în pluginuri. Core nu cunoaște numele DLL/provider. Metadatele lipsă rămân explicit necunoscute, nu se fabrică dovadă de integritate raw. |
| `SourceGraphRuntime.cpp` | Încarcă explicit DLL AUX/CINEMA/Matrix/SRC; două ramuri hardcodate pentru Tidal și MusicTransport | Nu se importă. Înlocuit cu registry de pluginuri și selector bazat pe catalog. |
| `SourceBlockExecutor.cpp` | 12 intrări Matrix, 10 ieșiri; roluri MUSIC/AUX/CINEMA | Nu se importă. Executor stereo și metadate numai MUSIC/AUX. |
| `SourceStateCore`, `SourceAppCore`, `SourceUi` în CMake | Executabilul vechi leagă Qt și Core în același proces | Două executabile: Core MSVC fără Qt și UI Qt separat prin IPC. |

DLL Matrix al tagului: `8f2e3b0109758003ea9a273fe078ad627d690d9f68fbdca72637e0ec626debb9`. DLL SRC al tagului: `443683eaff132843d34a53684aee6055afa08738fc9ad3f73c5d63a88435e710`. Se păstrează doar ca referințe offline; noul Core nu le folosește ca limite DSP.

Documentația Matrix locală descrie o versiune de sumare directă; codul local citit expune contribuții separate. Pentru extragere contează traseul efectiv al tagului: Matrix aplică mute, apoi ProgramMixer însumează. Nu se însumează de două ori.

## Contractul Stage1 → Stage2 există deja

Autoritate: `D:/Projects/3Stage-3/Stage2/contract/MusicStageBusContract.json`, SDK-ul și `MusicStageBusHashes.h` aferente. Nu există `WAITING FOR ABI`.

| Câmp | Valoare |
|---|---|
| StageBus ABI / transport | `0x00010000` / `StagePublishV2` |
| Layout de intrare Stage2 | `MUSIC_STEREO2_V1`, ID **3**, 2 canale |
| Plane | `MUSIC_L`, `MUSIC_R`, FP64 IEEE754 little endian, planar |
| Rată / bloc | **192000 Hz / 1024 cadre** |
| Session / block header | **256 / 128 bytes** |
| Plane stride / logical slot stride | **8192 / 16512 bytes** |
| Session capacity / slots offset / logical mapping bytes | **4 / 4096 / 70144** |
| Contract hash | `8d1dd0d0b733489a83ebb5a84ed287ad211175d27d7fbea64fa969130f55318e` |
| Layout hash | `4dc3f68562b29fdd2ab36e22c2d5a1df5f8b9ccfec8dba5095aa0b951a7f4448` |
| Endpoint implicit | `\\.\pipe\Entropy.StagePublish.Music.Source.V2.<SID>` |
| Roluri / mask | MUSIC=1, AUX=2; numai mask **3** |

Capacity 4 și mapping_bytes sunt valorile contractului StageBus, validate ca atare. Queue-ul efectiv V2 are separat 16 sloturi și storage maxim prealocat de 11 plane, conform ABI-ului transportului existent. Stage1 declară și copiază exact două plane; storage-ul rezervat nu introduce alte canale în program.

Identitate: UUID de sesiune nenul, QPC frequency real, `stream_epoch != 0`, `control_revision != 0`, `block_sequence` începe la 1, `first_frame = (block_sequence - 1) * 1024`. Restart Core creează UUID nou; reconnect Stage2 nu schimbă sesiunea Core. Se păstrează source generations și source rates pentru MUSIC/AUX; al treilea element și reserved sunt zero. Validity/discontinuity nu folosesc bitul CINEMA. Mute nu fabrică sursă validă și nu este echivalat cu lipsa providerului.

Publicarea nu conține demand/lease/control upstream. Core continuă fără consumator; un abonat lent pierde local pachete și se reconectează fără resetarea sursei. `service()`/conectările se execută în control, cu ownership sincronizat în afara pasului audio; `publish()` doar pe ownerul audio. Nu se introduce acces concurent nesigur la queue.

## Fișiere noi, limite și ordine

Directorul nou va fi `D:/Projects/3Stage-3/Stage1`, separat de Stage1 activ și de checkout-urile de referință.

- `Engine/Core`: lifecycle, scheduler, owner MUSIC/AUX, configuration.
- `Engine/Ingress`: Windows endpoint catalog/notifications, provider registry, selector, validare PCM și conversie FP64.
- `Engine/SRC`: wrapper stereo pentru aceeași implementare r8brain, două instanțe independente.
- `Engine/Matrix`: numai MUSIC/AUX mute/routing și ProgramMixer stereo.
- `Engine/Transport`: FIFO bounded și publisher MUSIC2.
- `Engine/IPC`: control local și snapshot de telemetrie; fără dependență Qt.
- `ABI`: provider C ABI V2, control IPC V1, wrapper al contractului MUSIC existent.
- `Providers/Legacy`: pluginuri de referință pentru adaptoarele actuale, cu limite de metadata declarate.
- `Providers/WindowsCapture`: captură eCapture selectată explicit; DAC eRender obișnuit nu este oferit ca ingress Exclusive.
- `UI/Qt`: client selector, Matrix mute și telemetry. Închiderea UI nu oprește Core.
- `ThirdParty`: SRC verificat, licențe, SDK/transport copiate cu hashes.
- `tools`, `tests`, `docs`, `release`: build, trei verificări de acceptanță focalizate, manifest și raport.

Nu se șterge nimic din Stage1 vechi. Nu se copiază în noul target: CinemaSourceDomainStream, CINEMA SRC/WDL, decoder, vechiul SourceGraphRuntime/SourceBlockExecutor/SourceController, vechiul Qt host sau vechiul contract audio 10/11.

Varianta aleasă este extragerea componentelor native validate într-un Core nou. O copie a hostului vechi ar păstra dependențele Qt/CINEMA; rescrierea SRC ar modifica algoritmul. Aceste variante nu satisfac documentul de cerințe.

Proiectul driver `Entropy-Music-Render-V2` rămâne separat și urmează Core-ului. Folosește ABI-ul providerului pentru transport raw, identități noi și implementare WaveRT pornită din surse Microsoft verificate; nu devine dependență de build/link a Core-ului. Instalarea și comparația TIDAL live sunt pași ulteriori, cu aprobare explicită.

## Metadate, surse și limite de execuție

Providerul V2 raw validează exact rate, channels=2, FL/FR, container/valid bits, block align, byte rate și full subtype GUID. Setul minim este 44100/48000/88200/96000/176400/192000 × PCM16 și PCM24-valid-in-32 MSB aligned. Alte reprezentări sunt refuzate până la implementare verificată. Pluginul legacy poate furniza FP64 deja convertit, dar nu revendică verificarea byte-identică a PCM upstream.

Rata provine din formatul providerului, nu din timer sau profil. Schimbarea identității/generației/formatului invalidează FIFO și SRC numai pentru sursa afectată; se pregătește în afara procesării audio. La 192000 Hz se păstrează same-rate copy al implementării existente. Pentru samples finite acesta este exact; politica existentă NaN/Inf→zero nu se ascunde sub o afirmație de bit-exact pentru orice bit pattern.

MUSIC și AUX au ceasuri independente. FIFO raportează underrun/overrun; nu ascunde drift prin SRC dinamic, interpolare, sample drop sau padding nemarcat. Pentru un contributor activ fără bloc complet, schedulerul publică numai contribuțiile complete, marchează lipsa/discontinuitatea explicit și nu amestecă generații într-un bloc. Cadrele sintetice de zero nu sunt raportate ca audio valid primit. Absența ambelor surse poate produce blocuri zero cu validity=0 pentru continuitatea programului.

Providerii sunt descoperiți la control din pluginuri versionate și catalog IMMDevice; alegerea exactă persistă separat în config candidat. Endpoint absent: UNAVAILABLE, fără fallback. AUX selection/change nu rebind/reset MUSIC. RAW nu este folosit drept dovadă pentru Exclusive; mode este UNKNOWN când nu există dovadă fiabilă.

WASAPI loopback funcționează doar Shared și nu acoperă ingress Exclusive: [Microsoft loopback recording](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording). Referințe driver: [WaveRT miniport](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/developing-a-wavert-miniport-driver), [Simple Audio Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/simpleaudiosample).

## Mediu și verificări ale acestei faze

- VS Community 2026 `18.10.12201.205`; MSVC x64 `14.52.36725` prezent.
- CMake **3.30.5**, Ninja **1.12.1**, versiuni executate și confirmate.
- Windows SDK/WDK `10.0.28000.0`: `portcls.h`, `portcls.lib`, driver props/targets prezente. Build-ul driver, integrarea toolsetului WDK și semnarea rămân **NOT RUN**.
- Qt 6.11.2 instalat ca `mingw_64`; nu există pachet Qt MSVC observat. Core se construiește MSVC fără Qt. UI poate folosi toolchain-ul Qt existent fiind alt proces, cu wire IPC independent de C++ runtime.
- Stage1 build/capture/live și instalare driver: **NOT RUN** în Faza A.
- Stage2 și Stage3 nu au fost lansate sau modificate în această fază. Modificările runtime Stage3 preexistente rămân în checkout.

Planul următor acoperă Core + adaptoare + Qt IPC. Driverul are plan/build separat după validarea Core-ului; rezultatul offline nu va fi prezentat ca validare WASAPI Exclusive live.
