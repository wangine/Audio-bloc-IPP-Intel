# Stage2 MUSIC v0010 — Dynamic Recovery stable

Această ramură conține sursa Stage2 MUSIC v0010 confirmată local și modulul nou
`EntropyDynamicRecovery.dll`. Lanțul L/R este EQ → Dynamic Recovery → LRC
Alignment; Center continuă să primească L/R brut. Modulul este **OFF implicit**,
inclusiv la încărcarea profilurilor vechi, iar pagina sa din graful Entropy oferă
control pentru transient, upward expansion și wet mix.

Pachetul Windows verificat este în
`release/stage2-music-v0010-dynamic-recovery-stable`, generat cu
`tools/Package-Stage2Music.ps1`. ZIP-ul cu același nume este atașat versiunii
GitHub. Pentru sursa Faust, ABI, SHA și rezultatele testelor vezi `REPORT.md`
și `source/Modules/EntropyDynamicRecovery/README.md`. Runtime-ul v0010 original
nu este modificat.

## Istoric: Stage2 MUSIC v0010 — LOAD / SAVE / auto-restore

v0010 adaugă LOAD lângă SAVE și memorează calea ultimului profil salvat sau
încărcat cu succes în `state/LastProfile.json`. La pornire citește fișierul
`.Edsp` original din acea cale; dacă lipsește ori este invalid, folosește copia
validă `state/Stage2State.json` și afișează motivul. Anularea dialogului sau un
profil invalid nu schimbă parametrii curenți și nici selecția memorată.

Profilurile noi salvează și activarea LRC Alignment. Restaurarea folosește
APPLY existent după ce workerul are latențe valide și Center este confirmat;
întârzierile sunt calculate pentru parametrii încărcați. Nu se serializează
istoricul audio sau întârzieri vechi devenite nevalide. Profilurile vechi fără
acest câmp rămân compatibile: LOAD nu schimbă alegerea LRC curentă, iar la o
pornire nouă LRC rămâne în bypass până la APPLY manual.

Pachet local nou: `release/stage2-music-v0010/ColdStart-DSP-Music.cmd`.
v0009 rămâne referința existentă, fără suprascriere. Stage1, Stage3, contractele
audio și toate DLL-urile DSP rămân neschimbate. LOAD folosește mailbox-urile
de control existente; nu face citire de fișiere pe threadul audio.

Verificări țintite: DspProfileUi, DspState, DspEqState, DspCenterState și
DspProfileRestartTests pe pachetul v0010, cu sursă StagePublish izolată.

## Referința MUSIC v0009

Stage2 păstrează modulele DLL și rutarea DSP din perechea acceptată v0008.
Contractele pasive sunt acum exclusiv MUSIC: **2ch IN / 3ch OUT**, FP64 planar,
192000 Hz, 1024 frames. Ordine: L/R la intrare, L/R/CX la ieșire.

EQ procesează L/R, Center primește separat L/R brut, iar Delay Align lucrează pe
L/R/CX. DLL-urile nu sunt recompilate sau modificate. Loudness rămâne în starea
baseline BYPASS / INACTIVE; această versiune nu implementează activarea lui.

## Directoare

- `source`: sursele Stage2 extrase din release 0.1.9, cu patch-ul MUSIC.
- `transport`: copie locală a StagePublishV2.
- `contract/MusicStageBusContract.json`: contractul MUSIC distribuit separat.
- `runtime/stage2`: dependențele native verificate, folosite pentru teste.
- `build`: compilarea locală independentă.
- `evidence`: baseline, RED/GREEN, verificări și raport.
- `release/stage2-music-v0009`: pachetul candidat cu executable, SDK și manifest.

## Contract și compatibilitate

Layout ID 3: MUSIC_STEREO2_V1; layout ID 4: MUSIC_LRC3_V1. Hash-ul contractului
MUSIC și hash-urile layout-urilor sunt generate de `tools/Update-MusicContractHashes.ps1`.
ID-urile 1/2 și contractul legacy sunt păstrate numai pentru fixtures și migrare
explicită; Stage2 MUSIC refuză intrarea legacy 10ch.

Semantica metadata MUSIC/AUX este păstrată, cu mask maxim 3 și source_generation[2]
rezervat zero. Stage2 copiază header-ul MUSIC valid fără renumerotare. Publicarea
rămâne pasivă: abonatul nu comandă sau oprește sursa.

ABI-ul wrapperului StagePublishV2 și storage-ul maxim prealocat rămân intacte.
Sesiunile MUSIC declară/copiază exact 2/3 plane; storage-ul neutilizat nu reprezintă
canale CINEMA și nu este payload audio declarat.

Endpoint-uri distincte de cele active vechi:
`\\.\pipe\Entropy.StagePublish.Music.Source.V2.<SID>` și
`\\.\pipe\Entropy.StagePublish.Music.Dsp.V2.<SID>`.

Noul Stage1 MUSIC nativ v0002 publică stereo FP64/192 pe endpointul MUSIC.
Integrarea live cu Stage2 v0009 a fost verificată: 1.873 blocuri consecutive L/R/C
citite pasiv, cu semnal pe toate trei canalele și zero goluri de secvență.
Auditul Shared/Exclusive este în `../Stage1/docs/DRIVER-LIVE-AUDIT-20261011.md`.
Stage3 0.1.9 consumă încă 11ch; selectorul său MUSIC/CINEMA rămâne o etapă viitoare.

## Build și verificare

`tools/Build-Stage2Music.ps1 -SkipTests` compilează cu Qt 6.11.2 / MinGW 13.1.
`ctest --test-dir build -R DspMusic --output-on-failure` verifică contractele MUSIC.
Named-pipe IPC și fișierele temporare ale fixture-urilor necesită accesul normal
al utilizatorului Windows. Suitele legacy și rezultatele lor rămân în evidence;
nu reprezintă acceptarea unui nou contract MUSIC.

Recorder MUSIC3: per bloc, header StageBus de 128 bytes și trei plane FP64 de
8192 bytes, în total 24704 bytes. Extensie recomandată `.music3`; fișierul nu este
compatibil cu înregistrările istorice 11ch. Sesiunea/layout-ul se păstrează în
raportul de captură, deoarece header-ul de bloc nu conține layout ID.

`tools/Package-Stage2Music.ps1` creează un output nou și refuză suprascrierea unui
pachet existent. `EntropyDspEngine.exe --verify-only` verifică SDK/contracte,
manifestul și DLL-urile înainte de pornire.

Profilul pachetului este local; manifestul exclude `state/` și diag/stop files.
Pachetul nou folosește valorile inițiale existente ale motorului dacă nu are profil;
nu pretinde că reproduce editările nesalvate ale instanței active.
