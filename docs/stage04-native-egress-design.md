# Stage04 — integrarea egress nativ pentru primul DAC

Baseline: `429589ad0831f034384b345eb6f3b875a8b49888`.
Branch: `codex/stage04-native-egress`. Document de design; implementarea și
validarea Stage04 nu sunt încă executate.

## Contract

AudioGraph FP64/192 → FIFO SPSC → ClockSync original → Guardian bypass exact →
PCM24 original → WASAPI Exclusive. Fără Stage2, Qt, modificări DSP, playback
automat sau push. Profilul FIR8193 × 16 este respins la ARM, indiferent de
rezultatul unui benchmark nou. Stage3 stabil rămâne neatins.

## Integrarea aleasă

Biblioteci statice C++20 pentru ClockSync, SRC nominal, DeviceManager și WASAPI,
cu sursele existente păstrate byte-identic. SRC folosește
`Dependencies/SRC/src/MultiChannelSrcEngine.cpp`, R8Brain și SoXR originale;
dependențele lor lipsesc din build-ul CMake actual și trebuie rezolvate cu
proveniență verificabilă. Nu se înlocuiește niciun SRC pentru a simplifica build-ul.

Hostul nou de egress deține lifecycle-ul și două thread-uri distincte:

- Graph Thread: generează cele 13 intrări offline/interne, execută AudioGraph,
  selectează canalele declarate pentru primul DAC și publică FP64 în FIFO.
- DAC Sink Thread: consumă FIFO, alimentează ClockSync și execută render,
  bypass Guardian, PCM24 și submit WASAPI. Acesta deține toate apelurile COM,
  open/start/wait/submit/stop/close pentru sink; nu se transferă ownership COM.

ClockSync și servo-ul primesc poziția și timestamp-ul DAC-ului, nu clock-ul
producătorului. Fiecare instanță egress are propriul FIFO, ClockSync și contoare.
Inventarul DeviceManager se actualizează în afara procesării. Refresh-ul și
reconfigurarea nu rulează în Graph Thread sau în intervalul de render.

Alternative evaluate: folosirea directă a ringului intern ClockSync drept FIFO
ar reduce un buffer, dar nu ar exprima separat contractul FIFO → ClockSync cerut.
Un adapter DLL ar adăuga o limită de module inutilă. Se alege adapterul static
cu FIFO explicit, fără modificarea originalelor.

## FIFO și lifecycle

FIFO planar prealocat, capacitate power-of-two, indici producer/consumer cu
release/acquire și atomici lock-free. Nu se suprascrie audio necitit. Push/pop
verifică pointeri, capacitate, frames și date finite. Overflow/underflow produc
contoare XRUN distincte și o politică de fault vizibilă; nu dispar din raport.
Publicarea unui bloc este completă înaintea actualizării write index.

Stări: DISARMED → PREPARED → ARMED → PREROLL → RUNNING → STOPPING → DISARMED,
cu FAULT din orice stare activă. Pregătirea prealocă toate resursele și face
warmup înaintea procesării. ARM verifică configurația, headroom-ul, profilul,
formatul driverului și autorizarea explicită hardware. Startup-ul preumple
bufferul; ieșirea rămâne silențioasă cât timp ClockSync nu este pregătit.

DISARM sau eroare publică imediat cererea de silence, oprește submit-ul de audio
util și închide sink-ul pe owner thread. Graph Thread nu așteaptă și nu face
device I/O. Join-urile și reseturile se fac numai după oprirea ownerilor.
Resetul FIFO/ClockSync nu concurează cu push/render. Device loss nu pornește
automat un endpoint substitut: recuperarea necesită prepare și ARM noi.
Pentru un endpoint pierdut nu se poate garanta livrarea fizică a unui buffer
silențios; se garantează oprirea trimiterii datelor utile și fault latched.

## Headroom și PCM24

Unity rămâne numai în fixture-ul explicit offline necesar comparațiilor istorice.
Traseul fizic cere o politică Headroom explicită, cu ledger și configurație
validate, aplicată prin motorul original înainte de ARM. Nu se declară arbitrar
`bounded=1` pentru un profil necunoscut și nu se consideră o măsurare finită
dovadă a unui bound pentru toate semnalele posibile.

Prima probă hardware este limitată la un profil intern de sinus cu amplitudine,
frecvență, rutare și gains fixe, declarate și verificate offline. Alte intrări sau
profiluri fără bound valid sunt respinse. Nivelul probei și politica de atenuare
sunt explicite în configurație și raport, nu gain ascuns sau normalizare.
Policy warmup și tranziția Headroom trebuie încheiate înainte de RUNNING.

PCM24 packed LE sau 24 valid bits în container de 32 se selectează exclusiv
după acceptarea formatului exact de driver. Fără SRC suplimentar sau conversie
float de către WASAPI. Se păstrează converterul original. Înainte de conversie,
hostul verifică datele finite și limita reprezentabilă pentru configurația
de dither; dacă originalul ar face rail clamp, întregul bloc devine silence și
se raportează fault. Nu se folosește clipping drept mecanism de siguranță.
Prima probă folosește explicit dither NONE pentru comparații deterministe.

Guardian opțional înseamnă copie identică FP64 sau ocolire directă a bufferului;
nu se activează procesarea phase/AVX2 a Guardianului. Identitatea include `-0.0`.

## Validare și livrabile

Teste de integrare cu sink fake, fără inventariere sau deschidere de endpoint:

- FIFO wrap, ordine, capacity, overflow/underflow și concurență reală SPSC.
- Startup/preroll, stop/DISARM, starvation, invalid PCM/nonfinite, device loss,
  erori open/start/wait/submit și recuperare fără audio vechi sau re-ARM automat.
- Sinus, impuls și zgomot determinist; PCM bytes identici cu lanțul original
  executat ca oracle separat; amplitudine, fază, latență și semnal activ explicit.
- ClockSync 192→192 și nominal 192→96 prin SRC original; clock drift pe fiecare
  domeniu; conservarea stării și a coeficienților originale.
- Validarea și respingerea ledgerului Headroom; respingerea FIR8193 × 16 la ARM.
- Bypass Guardian bit-exact și formatele packed24/24-in-32; fără rail clamp.
- Instrumentare allocations/deallocations/locks în Graph și render separat.
  Wait-ul evenimentului WASAPI este permis numai DAC Sink Thread și raportat
  separat de costul DSP. Nu se pretinde zero waits pentru schedulerul WASAPI.

Se execută testele existente plus testele Stage04, MSVC x64/IPP local. Benchmark
offline pentru 128/256/512/1024 frames: Graph, FIFO, ClockSync, PCM24 și lanțul
complet, mediană/p99/maximum, cu QPC și cicluri CPU separate. Scheduling exact
în µs rămâne BLOCKED dacă nu există acces CSwitch. FAIL-urile și toleranțele
originale rămân vizibile; acceptarea formală V07 rămâne distinctă.

Modul hardware este un executabil/CLI separat cu endpoint ID și confirmare
explicită, acceptat numai după raport offline PASS pentru configurația exactă.
Fără device discovery, open/start sau playback în testele automate. Instrucțiunile
primei probe precizează formatul, canalele, nivelul, durata și DISARM-ul.

Raportul final include build/test logs, rezultate numerice și CPU, hash-uri ale
surselor originale și executabilelor, limitări și instrucțiuni hardware.
Binarele, SDK-urile și rezultatele brute rămân locale. Fără push sau merge.
