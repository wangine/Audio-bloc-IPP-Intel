# Stage1 AUX — investigație după driverul MUSIC

## Rezultat actual, 11 octombrie 2026

Investigația autorizată a fost executată într-un Stage1 nou, **v0003-r3**;
v0002 rămâne referință. [Raportul curent](../../validation/music-render-v2/LOADED-DRIVER-0.3.0.11.md)
și [sursele/acceptarea Stage1](../../Integration/Music/Stage1-v0003) descriu rectificările.
AUX Windows Capture drenează GetBuffer înainte de a aștepta un eveniment, verifică
ReleaseBuffer și nu folosește GetNextPacketSize în Exclusive. Intrările folosesc
MMCSS Pro Audio, iar controlul păstrează ultima telemetrie coerentă la coliziune.
Formatul nu mai este șters la fault; o discontinuitate reală rămâne fault explicit.

Baseline Shared: salt nemarcat353cadre la67,53s. După rectificare: Shared120s,
5.292.441cadre fără salturi; TIDAL+Spotify Shared300s, fără faulturi/underrunuri Core,
snapshoturi goale de format sau regresii de contoare. Exclusive MMCSS120s,
5.292.000cadre fără salturi; proba anterioară fără MMCSS a eșuat și se păstrează.
Nu s-a reinstalat sau modificat vechiul driver AUX; durata verificărilor este finită.

Textul de mai jos păstrează ordinea și observațiile inițiale din etapa v0002.

Raport utilizator, 2026-10-11: a doua intrare, AUX, ajunge în FAULT după aproximativ
un minut atât cu Shared, cât și cu Exclusive selectat; interfața afișează
`access UNKNOWN`. MUSIC funcționează momentan fără fault.

Acestea sunt observații ale utilizatorului, nu o reproducere nouă sau un diagnostic
al cauzei. În adaptoarele legacy, UNKNOWN este deja folosit pentru metadate de
mod indisponibile; afișarea lui nu dovedește singură că el produce faultul.
Nu se atribuie acum problema driverului, Provider-ului, SRC-ului sau Core-ului.

## Ordine stabilită

1. Construim driverul MUSIC independent și Provider V2 raw pentru contractul existent.
2. Verificăm PCM integral și clock/continuitate în modurile și formatele acceptate.
3. Investigăm AUX și pregătim o versiune nouă Stage1 pentru remedierea demonstrată.

Stage1 v0002 rămâne referință în etapa driverului, cu surse și arhive nemodificate.
Core-ul poate fi modificat în etapa AUX; limita actuală nu este permanentă.
Până atunci nu comutăm AUX, nu schimbăm configurația live pentru reproducere și
nu introducem recuperare care ar putea ascunde cauza sau pierderi de date.

## Dovezi de colectat în etapa AUX

- Endpoint și Provider exacte, mod solicitat și mod efectiv cunoscut/necunoscut.
- Timp până la fault, cauza explicită și starea MUSIC în același interval.
- Format, generație, secvență, firstSourceFrame, lostFrames și discontinuități,
  numai pentru câmpurile disponibile real în provider; nu se fabrică metadate legacy.
- Delimitarea dintre eroare de read, invalidare/reconfigurare, FIFO/SRC și lifecycle.

Remedierea trebuie demonstrată printr-o reproducere înainte de fix și verificare
după fix, în Shared și Exclusive, cu durata și numărul de cadre consemnate.
MUSIC se verifică independent și simultan cu AUX, pentru a exclude o regresie
sau propagarea faultului între domenii. Nu se declară acum nicio cauză sau rezolvare.
