# Etapa 05 — Headroom explicit pe program și overrange real

**Scop:** elimină asimetria detectorului și verifică eventuala depășire rămasă fără a rescrie PCM24 ori a atribui automat mediile slabe lui AUTO.

## Dovezi

Masca detectorului a fost deja corectată, iar grupuri neacceptate sunt respinse explicit. NU relua aceste reparații.

`AudioGraph::process` cheamă `analyzeFinal` numai când `program == MUSIC`. În CINEMA, Headroom procesează cu starea/politica existentă, fără analiza blocului final curent. Schimbările ledger-ului pot modifica ținta; nu este obligatoriu un „gain înghețat”, dar nu este AUTO cu detector activ pe CINEMA.

Replay-ul publicat 0.1.4 arăta 1310 mostre overrange / 16 blocuri clipped, peak1,47956 pe DAC A, dar SUB numai0,813 și Headroom BIT PERFECT, pre/post identic. În intervalele de comutare live0.1.5 contoarele sunt zero. Nu declara clipping prezent astăzi numai din raportul vechi.

## Execuție

1. Folosește exportul real din etapa04 pentru mode și currentGain. `manualGainDb` este o valoare de control, nu dovada că acel gain este aplicat în BIT PERFECT.
2. Definește politica vizibilă: AUTO selectat trebuie să analizeze același semnal final post-Matrix/pre-gain indiferent de MUSIC/CINEMA. Elimină condiționarea pe program pentru analiza finală. Păstrează detectorul masked global, scalar comun, attack/release/limita AUTO existente; nu introduce limiter/look-ahead sau o rezervă nouă ascunsă. În MANUAL/BIT PERFECT analiza poate actualiza meterele, fără a modifica gain-ul determinat de modul ales.
3. La tranziția de program, detectorul vede efectiv blocul crossfade emis de Crossover, nu presupune un singur tip de sursă și nu folosește peak-ul unui bloc MUSIC vechi pentru a certifica CINEMA. Păstrează reviziile coerente la graniță de bloc.
4. Nu elimina `force(safetyTarget)` într-un patch cosmetic: modificarea rampei dinamice fără o altă politică de siguranță poate readuce overload. Calitatea AUTO trebuie analizată numai dacă AUTO era activ în problema reclamată. Tranzițiile fără salt și protecția pentru vârfuri arbitrare cer un contract separat; nu le masca prin clamp nou.
5. Citește contoarele PCM deja existente ca DELTA pe fragmentul reutilizat. Dacă cresc, identifică ieșirea fizică și primul tap care depășește domeniul: Crossover/Matrix/Headroom/ASRC. Refolosește datele etapei04. Dacă nu cresc, încheie această investigație; nu reface ditherul.
6. Nu confunda rail-clamp de rotunjire/dither lângă full-scale cu overrange FP64 mare. Nu confunda sample-peak safe pre-ASRC cu garanție true-peak. Current true-peak este declarat neimplementat; nu redenumi un peak simplu „true peak”.
7. Dacă o valoare de profil/gain produce overload în BIT PERFECT, raportează acel canal și marja lipsă. Eventuala comparație cu atenuare fixă se face într-un profil separat, autorizat și cu nivel analogic echivalent. Nu transforma modul bit-perfect într-un limiter ascuns.

## Verificare minimă

Un target offline cu aceeași secvență de vârfuri în MUSIC și CINEMA, același routing și aceeași stare inițială: AUTO trebuie să aibă aceeași decizie pentru aceleași mostre post-Matrix. În același target, MANUAL și BIT PERFECT păstrează exact multiplicatorul/identitatea după stabilizare. Adaugă în secvență un singur switch cu coadă de gain pentru a exclude contaminarea de stare între programe. Fără suită completă și fără tonuri trimise automat la DAC.


## Surse inspectate

- [`Engine/AudioGraph.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/AudioGraph.cpp)
- [`Engine/Control/ControlPlane.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/ControlPlane.cpp)
- [`DSP/Headroom/src/HeadroomEngine.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Headroom/src/HeadroomEngine.cpp)
- [`Engine/Egress/NativeEgress.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Egress/NativeEgress.cpp)
- [`validation/stage015/numeric.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage015/numeric.json)
- [`validation/stage016/live-switch.json`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/validation/stage016/live-switch.json)
