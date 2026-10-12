# Stage1 v0003-r8 — restaurarea AUX

Data raportului: **2026-10-12**, conform datei clientului. Candidat local în `D:/Projects/3Stage-3/Stage1-v0003-r8`, derivat din sursa r7 publicată la baza repository `fd735c5420d81f4ec89920d56c9a4039ce8809e7`. Nu s-a făcut commit/push pentru r8.

## Cauza și traseul rectificat

AUX avea rolul de a primi orice aplicație care redă pe ieșirea implicită Windows **Entropy AUX**. Traseul original este:

```text
Aplicație Windows → Entropy AUX Virtual Driver
                  → EntropyAuxEndpoint.dll
                  → FP64 L/R → SRC AUX r8brain → Matrix
```

În r7, profilul folosea greșit `windows.capture.v2`, endpointul eCapture AUX și modul clientului de captură Shared. Acea rută nu este echivalentă cu bridge-ul privat original și nu confirmă modul Shared/Exclusive al Spotify. În măsurarea anterioară rectificării, L=R și Side=0 existau deja înainte de SRC. Asta localizează pierderea stereo înainte de r8brain, fără să demonstreze separat întreaga răspuns în frecvență reclamat.

Adaptorul legacy cerea obligatoriu atât EndpointApiV1, cât și extensia EndpointCatalogV1 cu anulare de citire. DLL-ul AUX original oferă V1, fără acel catalog. Era astfel ascuns din enumerația adaptorului. r8 acceptă explicit V1 pentru AUX și păstrează cerințele catalogului pentru celelalte adaptoare.

Referința de arhitectură citită este `D:/Projects/Entropy Codex Lucru/Modular 3 stages/source`, în special `Modules/AuxEndpoint`, `Shared/EndpointBridge/src` și `Host/EntropyQtHost/Core/SourceDomainRuntime.cpp`. Bridge-ul original este o proprietate KS privată cu GUID `{AAC8B513-9D3D-4B2A-91F2-5A56C86AE201}`. DLL-ul furnizează deja FP64 către Stage1; nu s-a introdus client WASAPI eCapture pentru AUX. Transportul V1 are conversie Q31→FP64 în modulul original; nu este noul transport raw PCM V2.

## Modificări

- `Legacyaux` enumeră o singură identitate fixă: `legacy.aux` / `entropy-aux:system-default`, mode4; deschide direct V1 original fără catalog/bind.
- Payloadul păstrează L/R FP64 exact. Planul R este compactat de la offsetul de capacitate V1 la offsetul de cadre efective V2, inclusiv pentru citiri parțiale.
- Blocul gol transmite starea IDLE/RUN din statusul V1, fără PCM fabricat. Anularea este atomică; disconnect/destroy se execută după oprirea workerului, fără distrugere concurentă cu citirea nonblocking.
- Qt nu mai instanțiază selector, selector de acces sau buton Select pentru AUX. Titlul identifică r8. Mesajul de catalog trunchiat urmărește enumerația completă, nu pagina STATUS.
- Profilul livrat și inițializarea unui director de stare nou leagă automat AUX original. Selecția MUSIC rămâne independentă.
- Snapshotul și toolul de monitorizare păstrează RMS L/R și Mid/Side înainte și după SRC, plus contoare și vârfuri. Sursele și probele sunt păstrate local.

Toate fișierele din `Engine` sunt identice prin SHA256 cu r7. Nu s-au modificat algoritmii SRC, conversia PCM, Matrix, FIFO sau lifecycle MUSIC din Core. DLL-ul raw MUSIC V2 și kernelul său sunt aceleași; nu s-a instalat alt driver. Stage2 și Stage3 au rămas oprite în această probă, conform instrucțiunii utilizatorului.

## Shared/Exclusive: ce se poate afirma

Adaptorul transmite Shared sau Exclusive numai când flagurile V1 confirmă explicit modul producătorului. În proba live de aici, driverul AUX original nu a transmis niciun astfel de flag: **mode0 / UNKNOWN**. Qt spune explicit că modul producătorului nu este raportat de driver. Nu se deduce modul din rata de eșantionare, din opțiunea Spotify sau dintr-un client de captură.

Afișarea automată reală a comutării Shared/Exclusive rămâne lucru necesar în telemetria driverului AUX și în contractul său de transport. Noul adaptor are deja transmiterea flagurilor și teste pentru aceasta; testele simulate nu certifică existența lor în driverul instalat. MUSIC V2 păstrează aceeași limită de acces neconfirmat din r7.

## Dovezi

| Verificare | Rezultat | Fișier |
|---|---|---|
| V1 fără catalog pe adaptorul anterior | FAIL așteptat: AUX ascuns | `evidence/aux-bridge-red.txt` |
| Regresie V1 și suite Core finale | 7/7 PASS | `evidence/r8-final-ctest.txt` |
| V1 stereo, bloc parțial 2048/4096, IDLE, acces confirmat simulat, cancel | PASS | `tests/AuxBridgeRestorationTests.cpp` și testul CTest de mai sus |
| Qt anterior cu selector AUX | FAIL așteptat | `evidence/aux-ui-red.txt` |
| Qt r8 și runtime împachetat | PASS; două deschideri UI, aceeași sesiune Core | `evidence/aux-ui-green.txt`, `evidence/aux-ui-packaged-green.txt` |
| Config absent, launcher r7 | FAIL așteptat: AUX neales | `evidence/aux-cold-config-red.txt` |
| Config absent, launcher r8 | PASS cu V1 simulat; fără driver audio real | `evidence/aux-cold-config-green.txt` |
| Spotify → AUX original, monitorizare live | 120,949 s; 119 snapshoturi; rezultate mai jos | `evidence/r8-aux-spotify-120s.jsonl` și `.summary.json` |

Proba live: formatul raportat de bridge **24 biți valizi în container32, 44.100 Hz**, generație5 unică, **5.287.149 cadre noi la intrare** și **23.018.880 cadre noi la ieșirea SRC** între primul și ultimul snapshot. Delta lostFrames, overruns, underruns și faults: **0**; contoarele erau zero și la început. Source clock QPC 10.000.000 Hz; clockLateBlocks și publisherDrops au rămas zero. Sesiunea Core a rămas aceeași.

Side înainte și după SRC a fost nenul în **119/119 snapshoturi**, iar L/R distincte: ruta restaurată păstrează componenta stereo a piesei Spotify observate. Diferența numărului de cadre și ferestrele RMS nesincronizate la 44,1→192 kHz nu sunt amplificare și nu reprezintă o comparație eșantion-cu-eșantion a nivelului. Măsurarea live și contoarele V1 nu certifică identitatea PCM brut, lipsa oricărui defect pentru orice durată sau echivalența Spotify/TIDAL Shared/Exclusive. Noile verificări sunt distincte de rapoartele istorice r6/r7 pentru ruta eCapture greșită.

## Hashuri păstrate

| Artefact | SHA256 |
|---|---|
| `EntropyAuxEndpoint.dll` original | `F934B7BFB4E326F3451B6CB0D9C925750173E7A57A5A2A046E53374EA472985A` |
| `EntropyMusicRawProviderV2.dll` neschimbat | `3329D657956A42A6E83C28850EE457D1A3A9BF9AC327955825621732B6C4B0DE` |

Hashurile complete ale runtime-ului și surselor sunt în manifestele arhivelor locale. Fișierele de stare active și PID-urile nu sunt folosite ca dovadă imuabilă de acceptare.

## Starea finală a acestei etape

Traseul AUX original este restaurat și verificat pe proba de mai sus. r8 rămâne pornit pentru proba utilizatorului. Stage2/3 nu au fost pornite. Nu s-a făcut upload Git. Confirmarea automată Shared/Exclusive din driver și evaluarea auditivă completă rămân de verificat separat.
