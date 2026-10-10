# Etapa 04 — identificarea cauzei mid/midlow fără efecte compensatoare

**Scop:** găsește prima divergență neintenționată de amplitudine, fază sau continuitate, apoi corectează numai acel mecanism. Nu promite „rich dynamics” printr-un algoritm nou.

## Indiciu de verificat, nu setare de șters automat

`validation/stage013/actual-lanes.csv` are all-pass manual 1 kHz / Q≈0,71 pe CH1/2/3 și CH5/6/7, dar nu pe Center CH4. WAY1 are HP80,1/LP2040,6, WAY2 HP775,6/LP8604,7. SUB este FIR4097 cu AP110,4; SCOOP avea filtrele oprite. Fișierul este istoric. Citește profilul actual înainte de orice concluzie.

All-pass-ul manual `allPass/apFreq/apQ` nu este Thiran `fractionalDelayMode`. La 1 kHz AP-ul manual are 180°; comun tuturor căilor LR nu strică faza lor relativă, dar schimbă raportul față de Center/Scoop. Suprapunerea electrică dintre WAY1 și WAY2 poate fi intenționată pentru difuzoarele reale. Nu o declara eroare de cod fără dovadă.

## Execuție

1. Exportă o singură dată starea aplicată, nu doar cea cerută: scene, routing Matrix și canale fizice DAC; toate gain/delay/AP/fractionalDelay/HP/LP/FIR; Headroom mode/currentGain/target; EQ Stage2 bypass/static/dynamic, Center și LRC alignment. Generează `ACTIVE_AUDIO_PATH.md`, tabel compact cu fiecare ieșire și sursa ei. Profilul original rămâne nemodificat.
2. Recuperează fragmentul audio deja folosit în probele proiectului, dacă este disponibil. Altfel fă o singură captură FP64 scurtă (6–10 s) la ieșirea Stage2, din fragmentul indicat de utilizator, în memorie prealocată; flush după oprirea capturii. Nu introduce un recorder permanent. Înregistrarea se autorizează explicit și rămâne locală.
3. Refolosește ACELAȘI fragment în probe offline pe configurația activă. Capturează tap-urile exact o dată: input grafic, post-crossover, post-Matrix, post-Headroom, post-ASRC/pre-PCM. Păstrează frame index, epoch și, pentru ASRC variabil, read-position efectiv. Compară ferestre identice după alinierea delay-ului declarat; nu calcula un gain din RMS-uri cu cozi/preroll diferite. Nu compari bit-exact două ferestre aflate pe ceasuri diferite fără mapare temporală.
4. Pentru căile L/R liniare cu setări fixe, construiește transferul electric așteptat din coeficienții reali HP/LP/AP/FIR/Thiran și gain. Compară ieșirea cu acel transfer, nu cu intrarea raw. Reproduce numai o divergență găsită cu un burst scurt în banda afectată. Nu folosi o referință care cheamă aceeași funcție suspectă și apoi declară identitate.
5. Într-o COPIE offline a profilului, compară o singură variantă fără AP-urile manuale 1k pe LR, păstrând Thiran și toate celelalte controale. Compară separat LR cu ramurile Center/Scoop incluse în model; corelația și amplitudinea Center trebuie luate din audio, nu presupuse. Acest A/B este diagnostic, nu migrare de profil. O audiție cu Center/Scoop mute necesită acordul utilizatorului.
6. Verifică rolul EQ MUSIC LR: Center primește raw LR înainte de EQ în Stage2. Dacă EQ este bypass, îl excluzi; dacă este activ, incluzi transferul și eventualele dinamici în comparație. Nu recorecta Center ori muta tap-ul lui fără a demonstra că actualul contract este greșit.
7. Verifică dacă ramurile care se însumează acustic sunt pe același DAC. Pentru căi împărțite A/B, folosește contractul temporal din etapa03. Pentru căi în același DAC, nu atribui problema generic ceasului celuilalt DAC.
8. Raportează pentru banda afectată: nivelul și transferul complex, peak/RMS/crest factor pe aceeași fereastră, modificarea envelope-ului față de referința intenționată și existența oricărui salt de frame. Crest factor diferit după un filtru nu înseamnă automat compresie; poate rezulta din filtrare/fază.

## Decizie de corecție

- Divergență înainte de Crossover: oprește intervenția în Stage3; raportează tap-ul Stage2 și setarea/modulul implicat.
- Divergență față de transferul electric intenționat: repară exclusiv coeficienții, starea, gain-ul duplicat ori timestamp-ul demonstrat greșit.
- Transfer electric corect, dar sumă/model dependentă de AP/overlap: livrează profil diagnostic separat și rezultatele. Nu suprascrie alinierea acustică sau frecvențele originale.
- Problemă numai după ASRC: repară egress-ul, nu un EQ sau Center care o maschează.
- Nicio divergență identificată: raportează exact acest lucru; următorul discriminator este o singură comparație analogică simultană, nu încă o rescriere de DSP.

## Buget de verificare

O stare exportată, un fragment reutilizat, cel mult un burst pentru defectul identificat. Fără sweep-uri combinatorii pe toate modurile. Pentru orice cod corectat: o regresie a acelui defect, apoi build. Acceptarea auditivă aparține utilizatorului.


## Surse inspectate

- [`validation/stage013/actual-lanes.csv`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage013/actual-lanes.csv)
- [`DSP/Crossover/src/PluginProcessor.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/src/PluginProcessor.cpp)
- [`DSP/Crossover/src/FractionalDelay.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/src/FractionalDelay.cpp)
- [`Integration/Stage2/source/Host/EntropyDspEngine/Core/DspStageRuntime.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Integration/Stage2/source/Host/EntropyDspEngine/Core/DspStageRuntime.cpp)
- [`Engine/AudioGraph.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/AudioGraph.cpp)
- [`DSP/Headroom/src/HeadroomEngine.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Headroom/src/HeadroomEngine.cpp)
- [`validation/stage015/numeric.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage015/numeric.json)
