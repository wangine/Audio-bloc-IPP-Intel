# Stage1 Native MUSIC/AUX v0002 — candidat local

Core C++20 MSVC x64 separat complet de Qt. Două surse independente MUSIC/AUX, conversie PCM16 sau PCM24 valid în 32 → FP64, SRC r8brain separat pe sursă, FIFO stereo și publisher pasiv MUSIC2 la 192 kHz / 1024 frames. Cinema intră ulterior direct în Stage3 din decoderul său.

Rectificările utilizatorului din 10 octombrie au prioritate față de documentul original: mute și rampa au fost eliminate complet. O sursă activă este copiată la nivel unitar; două surse active se însumează fără normalizare. `None` deconectează sursa. Fără gain, limiter, dither sau clipping.

## Pornire locală

Din folderul `Stage1`, dublu-click pe `Start-Stage1-Music.cmd`: pornește Core în fundal și deschide Qt. Un al doilea start reutilizează procesele deja pornite. Aceleași launchere sunt și în `release/stage1-native-music-v0002`, pentru utilizarea pachetului separat.

`ColdStart-Stage1-Music.cmd` oprește controlat numai Core-ul pornit de acest launcher, așteaptă închiderea și îl repornește. Păstrează selecțiile și profilul; nu reinstalează/resetază driverul. `Stop-Stage1-Music.cmd` oprește Core; clientul Qt poate rămâne deschis și se reconectează la următoarea pornire. Identitatea procesului este verificată prin cale, PID și ora creării; nu există `taskkill` global.

Pentru primul test, după oprirea vechilor Stage1/DSP: pornește cu `Start-Stage1-Music.cmd`, selectează intrarea exactă la MUSIC și apasă `Select`; lasă AUX pe `None`. Pornește redarea către endpointul virtual existent. Verifică clock-ul RUNNING, formatul raportat, creșterea contorului de pachete și cei trei indicatori de vârf. La prima pornire selecțiile din `state/Stage1Music.ini` sunt goale. `UNKNOWN` pentru biții originali ai unui adaptor legacy înseamnă că acesta nu oferă metadatele, nu că intrarea are 64 biți.

Acesta este testul de captură/publisher al Stage1. Pentru ascultarea lanțului complet trebuie conectate separat Stage2 MUSIC și Stage3; launcherul Stage1 nu le pornește. Driverul Music Render V2 rămâne proiect ulterior. Închiderea Qt lasă Core activ. Scripturile `Start-Core.ps1`, `Start-UI.ps1` și `Stop-Core.ps1` rămân disponibile pentru utilizare separată.

Selectorul enumeră pluginurile C ABI și dispozitivele Windows, păstrează identificatorul exact și raportează UNAVAILABLE fără fallback. MUSIC și AUX au selecții, FIFO, SRC și generații separate. Același provider/endpoint nu poate fi selectat în ambele roluri. Endpointurile eRender generice sunt vizibile și neeligibile; nu există loopback ascuns sau client Direct KS.

Intrarea arată cuvântul original, de exemplu `16/44.1` sau `24/96`; payloadul FP64 al adaptorului legacy nu este prezentat ca bit depth original. Metadatele necunoscute rămân UNKNOWN. Clock-ul Core este QPC + high resolution timer, independent de Stage2. Nu reprezintă validarea clock-ului kernel al viitorului driver.

UI raportează separat Sample Peak la intrare, True Peak estimat 4x și SRC Startup Peak. Definițiile/fereastra de stabilizare sunt în `docs/LEVEL-RESULT.md`. Aceste citiri nu modifică sunetul.

## Reproducere

`tools/Build-Stage1Music.ps1` compilează Core și rulează cele trei cazuri focalizate, fără Qt/WDK. `tools/Build-Stage1Qt.ps1` compilează separat Qt MinGW. `tools/Test-Stage1Qt.ps1` verifică IPC offscreen, cu sursă sintetică. `tools/Package-Stage1Music.ps1` creează o livrare proaspătă și arhivele cu SHA256.

`tools/Test-Stage1Launchers.ps1` execută launcherele CMD prin Windows PowerShell 5.1, pe Core-ul livrat, cu pipe propriu și selecții goale. Verifică pornirea repetată, profilul păstrat la cold start, refuzul unei identități de proces schimbate și oprirea controlată.

Toolchain verificat: MSVC 19.51.36257, CMake 3.30.5, Ninja 1.12.1, Qt 6.11.2 / GNU 13.1.0. Scripturile indică instalările locale `C:/Qt` și VS găsit prin vswhere.

## Starea livrării

Faza B Core + UI este candidat offline; detalii în `docs/LIVE-RESULT.md`. Legătura cu Stage2 MUSIC v0009 a fost verificată cu instanțe headless izolate și DLL-urile sale native. Vechea configurație ascultată rămâne intactă; revenirea este alegerea ei, nu reinstalarea unui driver.

Captura reală TIDAL prin Music Transport a fost observată la 16/44,1: 35 secunde fără pierderi noi, underrun-uri noi, overflow sau fault, cu clock la 192 kHz. Exclusive a fost raportat de utilizator; adaptorul V1 nu confirmă modul în telemetria V2. Spotify, comparația Shared/Exclusive, continuitatea pe durată lungă și commitul PCM din kernel nu sunt validate. Driverul `Entropy Music Render V2` nu este implementat/instalat aici: proiect separat descris în `docs/DRIVER-V2-HANDOFF.md`. Adaptoarele legacy sunt referințe temporare, nu dovadă de raw PCM V2.

v0002 corectează comenzile `Select` pierdute de clientul Qt în timpul pollingului și permite workerului de intrare să aștepte spațiu în FIFO la capturarea unui backlog. Schedulerul nu așteaptă; capacitatea rămâne 16×1024. SRC, nivelul și mostrele nu sunt modificate. Fiecare comandă se poate anula la oprire.

Hashurile per fișier acoperă fișierele livrate care rămân fixe; directorul `state` este mutabil și exclus. Hashurile arhivelor acoperă întreaga livrare proaspătă, cu profil inițial gol.

Surse SRC pin-uite: Audio-bloc-IPP-Intel `stage1-passive-v0002`, commit `48f979f489264885ab1a16351f81c43ddf64b0f1`; transport pin-uit din Stage2 MUSIC. DLL-urile ingress provin din surse locale și au fost potrivite cu hashurile tagului. Matrix/SRC DLL nu sunt încărcate de Core. Documentul original salvat este în `../docs/stage1/instruction(20261010-180432).md`.

