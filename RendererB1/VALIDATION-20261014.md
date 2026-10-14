# B1 — verificare locală din 14 octombrie 2026

Sursă: `chatgpt/stage3-renderer-b1-20261014`, commit `b56c5067ba22d0033ff43845a87914c65546bff4`. Această pagină consemnează **rulările deja făcute**; nu reprezintă o nouă execuție de teste. Codul testului punctual este în [Tests.cpp](Tests.cpp), iar limitele candidatului sunt în [DESIGN.md](DESIGN.md).

## Rezultat

| Etapă | Prima rulare în sandbox | Reluarea unică, cu acces punctual în contextul utilizatorului Windows |
|---|---|---|
| Configurare și compilare | exit 0; trei executabile B1 construite | exit 0; build incremental, trei pași |
| Testul `EntropyPacketB1Tests` | exit 0 | exit 0 |
| Pregătire PCM | exit 1: `weakly_canonical: Access is denied` la copia profilului | exit 0; 17.109 blocuri Stage2 procesate offline |
| Script `Build-B1.ps1` | exit 1, pachet marcat `INCOMPLETE` | exit 0, pachet nou complet |

Mesajul exact al testului la ambele rulări a fost: `PASS 48 packetization cases plus admission/header checks; no hardware qualification`. Cele 48 de cazuri acoperă 4 numere de canale × 2 lățimi de eșantion × 6 mărimi de pachet. Testul mai verifică finalul parțial, păstrarea ordinii octeților, refuzul admiterii/commitului greșit sau dublu și limitele headerului. Nu este un test WASAPI sau DAC.

Prima rulare a păstrat pachetul incomplet și logurile. Reluarea a folosit aceeași sursă, fără editări, și copia de profil verificată din pachetul inițial; nu a recopiat profilul curent din Documents. Nu s-au curățat buildul existent, schimbat permisiuni NTFS, rulat Administrator, pornit playerul ori controlat procese audio. Logul de pregătire reușită spune: `PREPARED offline: 17109 captured blocks; input copy exact; original DSP and PCM24 used. No DAC opened.`

Captura are 17.519.616 cadre sursă. Au fost adăugate explicit 192.512 cadre de intrare zero pentru coada offline; fiecare PCM pregătit are 17.712.128 cadre, 8 canale, 192 kHz, 24 biți valizi în container de 32 biți. `prepared.json` consemnează `inputCopyExact=true`, `liveInputVerified=false` și `audioQualityVerified=false`.

## Identități SHA256

| Artefact local | SHA256 |
|---|---|
| Captură Stage2 de intrare | `737dad1df5d0f2236219caa954c46d501e10254041acd4c51470d2beea89c051` |
| Copie de profil folosită | `f516153f44b2c968493260518b51f4c05e174d825c2e4d748f68b8c049ddb2c2` |
| `EntropyPacketB1Tests.exe` | `4567e332ef3660aa2eac0d2f6fe94d10633abc8ee4de7c38afe9409f3215446e` |
| `EntropyPlayB1.exe` din pachetul reușit | `d9ef3b358c4787b64723b5ab9fe254b76bb1867682076f6925fdcede1e994d71` |
| PCM DAC A | `653f685d6668fa14652d4c0b7eed537cc155f3c055ab256f59779e649bf3ebe3` |
| PCM DAC B | `7add91f6c69a963469d46d66838a052742263ce928544ae39bb5566c1d260a86` |

Hashurile PCM și ale profilului au fost recalculate și comparate cu manifestul pachetului reușit. În reluare, CMake a emis 15 avertismente de dezvoltare pentru politica `CMP0115`, iar compilatorul nu a emis avertismente. Prima compilare completă a avut și 3 avertismente `C4244` și 4 `C4324`; acestea nu au oprit buildul.

## Limită și verdict

Ascultarea raportată ulterior de utilizator a fost la nivel foarte mic, cu verdict **NECONCLUDENT**. Etichetele DAC A/B au părut inversate, dar inspecția profilului, matricei, manifestelor și launcherelor nu a găsit o inversare digitală. [Harta logică](MAP-LOGICAL-20261014.md) arată rutele fără a publica ID-urile endpointurilor. Corespondența cu mufele, amplificatoarele și difuzoarele fizice este **NECONFIRMATĂ**. Nu există aici dovada unui remediu auditiv sau a unei verificări a redării live.
