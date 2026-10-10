# Entropy AudioCore 0.1.5 — audit și intervenții separate

Data: 10 octombrie 2026.
Bază: `wangine/Audio-bloc-IPP-Intel`, `v0.1.5`, commit `3afb8d77b3bdd984a2f5bb53095cb99c37de72c0`.

Pachetul conține instrucțiuni și dovezi, nu un patch aplicat și nu un runtime verificat pe DAC. Auditul este al surselor accesate prin GitHub. Calculele FFT/frecvență/timing au fost reproduse separat; nu sunt rezultate MSVC, IPP sau WASAPI executate aici. Rapoartele din `validation/` sunt dovezi publicate de proiect, nu teste rulate din nou de autorul auditului.

## Concluzia de lucru

Independența Stage1/2/3 este realmente schimbată pe traseul StagePublish V2 citit. PCA are o reconstrucție nouă, fără vechiul câștig excesiv în cazul coerent. În schimb, calificarea offline a rămas în handler-ele live ARM/MUSIC/CINEMA, graficele folosesc grile incompatibile, iar ieșirile A/B nu au un contract explicit de prezentare a aceleiași cronologii audio. Cauza exactă a impresiei de medii slabe nu este încă demonstrată. Profilul istoric publicat conține all-pass-uri manuale la 1 kHz și benzi suprapuse, ceea ce trebuie separat de presupusele defecte de buffer.

## Ordine și limite

Execută un singur fișier numerotat odată, împreună cu acest README și `AUDIT_DOVEZI.md`. Ordinea recomandată este 01 → 02 → 03 → 04 → 05 → 06. Etapa 04 identifică prima divergență audio; nu autorizează o rescriere generică a filtrării. Etapa 03 are relevanță auditivă directă pentru ramuri distribuite între DAC-uri și relevanță arhitecturală pentru întregul egress.

Înainte de modificări, verifică HEAD, working tree, executabilul realmente pornit și profilul realmente încărcat. Nu presupune că `main`, ultima arhivă descărcată și EXE-ul activ sunt identice. Dacă funcțiile locale au fost deja corectate, raportează diferența și nu reintroduce implementarea veche.

Păstrează originalul 0.1.5 și profilul lui. Fiecare etapă produce un commit și un folder de runtime distinct. Fără push, merge, auto-ARM sau schimbări de hardware necerute.

## Reguli comune

- FP64 până la unica cuantizare finală PCM24 per DAC; TPDF flat, seed independent, Guardian bypass, sinc ClockSync și Thiran rămân.
- Cele două DAC-uri păstrează estimatoare/ASRC separate. O cronologie comună NU înseamnă un ppm identic forțat pe două ceasuri independente.
- Nu altera EQ, gain, polaritate, frecvențe, delay-uri, all-pass-uri sau routing în profilul original. Comparațiile folosesc copii explicit denumite.
- Fără EQ boost, exciter, saturator, normalizare sau compresor nou pentru a simula „rich dynamics”.
- Fără logging de mostre, JSON, scanare dispozitive, heap allocation, așteptare IPC sau benchmark în callback-ul de audio.
- Fără reluarea întregii suite CTest, soak tests de ore ori probe sintetice automate la fiecare comandă. O regresie țintită per defect și o verificare relevantă a candidatului sunt suficiente pentru aceste etape. Un rezultat contradictoriu se investighează, nu se ignoră.
- Testele/probele sunt executabile separate sau instrumentare opt-in pentru diagnostic, excluse din produsul normal. Nu confunda inițializarea legitimă a unui FFT pe thread de pregătire cu mii de blocuri simulate la apăsarea ARM.
- Păstrează validarea formatului, canalelor, identității, NaN/Inf, continuității și protecțiilor de ieșire. Eliminarea benchmark-ului nu autorizează eliminarea siguranței.
- Nu folosi tonuri ultrasonice puternice în difuzoare. Probele numerice sunt offline. O eventuală măsurare analogică folosește nivel redus și aceeași captură multicanal, cu autorizarea utilizatorului.

## Raport per etapă

Maximum 12 rânduri: cauza, funcțiile modificate, rezultatul verificării cerute, ce a rămas neschimbat, commit, calea EXE și hash. Separă „build reușit”, „defect reprodus/corectat”, „măsurare hardware” și „acceptare auditivă”. Nu scrie „dinamica rezolvată” pe baza unui build sau a unui contor de blocuri.
