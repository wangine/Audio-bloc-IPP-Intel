# Stage 3 — recuperare audio, lucru separat

Data: 13 octombrie 2026.

**Starea acestei livrari: separare si instructiuni de compilare. Nicio modificare DSP, nicio reparatie audio declarata.** Build-ul Windows si ascultarea nu au fost executate de ChatGPT. Un build al acestei revizii nu trebuie prezentat ca o versiune care suna mai bine.

## 1. Identitate si responsabilitati

- Repository: `wangine/Audio-bloc-IPP-Intel`.
- Branch de lucru: `chatgpt/stage3-audio-recovery-20261013`.
- Punct de pornire exact: `c2b1bc5060fc0c5efeaa7b1dacc84dcba4938ebb` — ultimul push indicat de utilizator, Stage3 0.1.9-v2-r3.
- Referinta de investigat: tagul `v0.1.0`, commit `48f979f489264885ab1a16351f81c43ddf64b0f1`.
- Identitatea dintre acest tag si executabilele locale notate de utilizator drept «0.1.0 beta 64/32» NU este inca verificata prin hash/provenienta. Nu o presupune.
- ChatGPT: analiza surselor si propunerea/modificarea controlata a codului in acest branch, in sesiuni explicite de lucru.
- Codex local: numai preluarea reviziei indicate, compilare si raportare. Fara diagnostic autonom, modificari de cod sau reluarea taskurilor istorice.
- Utilizatorul: evaluarea auditiva si autorizarea accesului la DAC.

Branch-ul original, main, tagurile, versiunile instalate si profilele raman neatinse. Un branch separat nu este un repository nou; pastreaza istoricul necesar comparatiei.

## 2. Ce stim si ce NU stim

Simptom raportat de utilizator: voce/mid si mid-low strangulate pe rock si orchestra in versiunea actuala; 0.1.0 beta a sunat relativ mai bine. Functionalitatea UI nu este problema prioritara.

Raportul atasat «Stage 3 r3 chain verdict — 2026-10-13» acopera o captura de 34,3 secunde, cu L way 1 solo, HP 80,1 Hz si LP 1692,6 Hz. In acel interval raporteaza continuitate si concordanta intre captura si replay-ul prin acelasi Stage3, dupa stabilizare.

Aceasta este dovada utila de repetabilitate si continuitate in intervalul analizat, nu o comparatie independenta cu raspunsul audio al versiunii 0.1.0. Un defect algoritmic comun executiei live si replay-ului poate exista in ambele. Solo-ul este valid pentru inspectia benzii, dar nu califica suma tuturor cailor. Raportul nici nu demonstreaza ca Stage2 este cauza; ii lipseste referinta independenta pentru acea limita.

Sursa `docs/RELEASE-0.1.0.md` de la tagul vechi cere Stage2 v0007-source-isolation si descrie 11 canale L/R/C + 8 CINEMA. Sursa actuala descrie intrari MUSIC3 si CINEMA6/8 independente. Prin urmare NU inlocuim orbeaste intregul Core vechi si NU schimbam Stage2 pentru aceasta pregatire.

`AGENTS.md` original continea instructiuni din etapele initiale, inclusiv build/CTest general si permisiuni de modificare a integrarii. A fost inlocuit numai in acest branch cu rolul BUILD-ONLY. Aceasta corecteaza organizarea lucrului, nu demonstreaza cauza sunetului.

## 3. Proiectul local nou

In Codex se deschide numai:

`D:\Projects\Entropy-Stage3-Recovery-20261013`

Din PowerShell, pentru un director care NU exista deja:

```powershell
$work = 'D:\Projects\Entropy-Stage3-Recovery-20261013'
if (Test-Path -LiteralPath $work) { throw 'Directorul exista. Nu il suprascrie.' }
git clone --single-branch --branch chatgpt/stage3-audio-recovery-20261013 https://github.com/wangine/Audio-bloc-IPP-Intel.git $work
if ($LASTEXITCODE) { throw 'Clone esuat; opreste-te.' }
Set-Location -LiteralPath $work
git branch --show-current
git rev-parse HEAD
git status --short
```

Se consemneaza SHA-ul complet primit. Fara pull, merge, rebase, cherry-pick sau schimbare de revizie in timpul compilarii. Fara copierea folderelor build/runtime din proiectul vechi.

## 4. Dependente locale explicite

Compilarea existenta foloseste MSVC x64, Intel IPP local si un arbore SRC care nu este inclus complet in checkout. `cmake/Stage04.cmake` compileaza din acesta inclusiv `MultiChannelSrcEngine.cpp` si dependentele SRC. Nu presupune ca un clone gol este autonom.

Copiaza NUMAI arborele de surse de dependente, fara executabile/profil/build, din:

`D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\local\stage04-ref\Stage3\native-reference\dependencies\SRC`

in noul checkout, la:

`local\stage04-ref\Stage3\native-reference\dependencies\SRC`

Sursa veche se citeste, nu se modifica. In raport consemneaza calea sursa si hash-urile fisierelor copiate. Nu descarca alta versiune SRC si nu substitui implementarea. Daca lipseste arborele sau SDK-ul asteptat, raporteaza lipsa si STOP.

Wrapperul curent are cai explicite pentru Visual Studio 18 Community, Qt Tools/CMake/Ninja si IPP din CMake. Nu edita scriptul sau CMake pentru a ascunde o dependenta lipsa. UI foloseste instalarea Qt/MinGW existenta; Core ramane MSVC.

## 5. Compilare limitata — 64k prima data

Din radacina noului checkout, in PowerShell:

```powershell
& .\tools\Build-MusicCinema.ps1 -FifoFrames 65536 -BaselineOnly -Targets @('EntropyAudioCore') -Parallel 2
if (-not $?) { throw 'Build Core esuat; raporteaza prima eroare si STOP.' }

& .\tools\Build-Stage05-UI.ps1 -BuildFolder 'build\qt-recovery'
if (-not $?) { throw 'Build UI esuat; raporteaza prima eroare si STOP.' }
```

Nu adauga `-Verify`. Nu apela `Build-Windows.ps1`, CTest, benchmarkuri sau capturi live. Wrapperul Core configureaza tinte de test existente, dar comanda de mai sus compileaza numai Core si dependentele lui; executa si `--probe` offline. Acest probe nu valideaza calitatea audio si nu autorizeaza ARM.

Rezultatele asteptate ca locatie, NU ca succes deja verificat:

- `build\msvc-music-cinema-65536\EntropyAudioCore.exe`
- `build\qt-recovery\EntropyUI.exe`

Nu compila inca varianta 32k. Nu amesteca variatia FIFO cu investigatia algoritmilor. Nu modifica FP64, politica floating point sau flagurile CPU.

Nu rula scripturile istorice de packaging: unele au destinatii si etichete de validare vechi. Aceasta livrare se opreste la executabilele compilate si raport; nu produce un pachet declarat reparat si nu necesita un nou test auditiv.

## 6. Raportul Codex — numai rezultatul operatiilor

Livreaza un singur `BUILD-REPORT.md` in folderul de lucru, cu: branch si SHA complet; comenzile rulate; rezultat configure/compile/link si probe; calea si SHA256 pentru Core/UI; provenienta dependentei SRC si SDK-urile; lista oricaror erori; `git status --short` final. Nicio modificare a fisierelor urmarite nu este acceptata. Raportul local si fisierele generate se declara separat.

La eroare: STOP, fara reparatii automate si fara o noua campanie de teste. Nu porneste sau opreste procesele Stage1/2/3, nu deschide DAC-uri, nu modifica profilul si nu face push/merge.

## 7. Urmatoarea interventie audio — in afara sarcinii Codex de compilare

Analiza se face pornind de la diferenta fata de referinta identificata, nu de la presupunerea ca toate modificarile recente sunt defecte. Se separa transformarile DSP, aplicarea parametrilor/profilului si egress-ul; se pastreaza functionalitatea actuala. Orice corectie trebuie sa precizeze cauza sustinuta de cod, fisierele schimbate si limita dovezilor.

Nu dezactiva protectii sau nu elimina Headroom/ClockSync ca presupusa solutie. Nu considera un contor de underrun, o captura fara erori ori egalitatea cu acelasi algoritm drept verdict suficient asupra regresiei auditive. Nu declara problema rezolvata fara comparatia relevanta si evaluarea utilizatorului.
