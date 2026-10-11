# Stage1 v0003-r3 — runtime și surse

Runtime ZIP: **EAA2979C0DB40DC0BE359FC8D71135F5523F2813756AC2E7DB35C6F941DC40DA**.
Source ZIP: **A249F0904936747C581141867A9AE2678BBCA744FA191C55CA403B6DEFFC8E5C**.
Core SHA256: **C36CE4148FC50BBF4DFA002AD7592BA50D4AF79A1ECF8960063BF30FB091A147**.
Source commit: `1034c5c` pe `codex/music-cinema-architecture`.

Extrageți runtime-ul și porniți `Start-Stage1-Music.cmd`. Profilul inițial din
această livrare privată păstrează selecțiile testate local: MUSIC legacy Transport,
AUX Windows Capture Shared. Identificatorii endpointurilor sunt locali acestei
mașini; pe alt sistem trebuie selectate dispozitivele locale. Qt se poate închide
independent de Core. ColdStart și Stop controlează numai procesul acestui launcher.

Sursele includ toate dependențele Stage1 și fixture-ul driverului. Pentru cele
**6 teste**, proba Architecture are nevoie și de **runtime-ul real Stage2v0009**,
la `../Stage2/release/stage2-music-v0009` față de folderul sursă Stage1; acesta este
deja inclus în repository la `Integration/Music/Stage2`. Nu este o dependență a
procesorului Stage1, ci proba de interop MSVC→MinGW pe componentele reale.
Reconstrucția arhivei a trecut 6/6 după furnizarea exactă a acestei referințe,
fără înlocuirea sau omiterea testului.

Executabilele IngressProbe/ControlProbe/OutputPublisherFixture și runnerul
raw/FP64 sunt păstrate în runtime, pe lângă sursele și rapoartele lor. Generatorul
se folosește numai cu pachetul Music Render V2 manifestat și Core/SHA explicit.
Identitatea PCM→FP64 este verificată la native 192 kHz; la celelalte rate SRC modifică
grila de eșantionare și este verificat diferențial. Consultați raportul de acceptare
din surse pentru durate, condiții și limite.
