# AudioCore 0.1.0 — test de izolare a sursei

Pereche obligatorie: **Stage2 v0007-source-isolation + AudioCore 0.1.0**.
Stage2 v0006 și Core 0.0.9 rămân păstrate separat. Nu combina noile versiuni cu transportul vechi.

Stage2 execută DSP o singură dată, la cererea locală nominală, și publică blocurile finalizate prin StagePublish V2. Core primește 11 canale planare FP64/192 kHz în ordinea L/R/C + 8 CINEMA. Nu cere procesare, nu deține lease-ul sursei și nu trimite Release/Retry/Reset către Stage1 sau Stage2. Qt comunică exclusiv prin IPC cu Core.

Pornire pentru test:

1. Închide vechiul Core și vechiul Stage2 înainte de a schimba perechea. Această schimbare de Stage2 poate roti sesiunea sursei; nu face parte din testul de independență al Core-ului.
2. Pornește `ColdStart-DSP.cmd` din folderul separat `versions/v0007-source-isolation`. Păstrează Stage1 în funcțiune.
3. Pornește `Live-Test.cmd` din acest folder AudioCore. Se lansează Core, apoi UI; Core începe DISARMED. Încărcarea implicită folosește profilul `Documents/entropy.entropy`; poți selecta alt profil prin UI sau parametrul `-Profile` al launcherului.
4. Verifică inputul și asignările DAC. ARM rămâne o acțiune explicită, cu validările existente; launcherul nu armează automat. `Cold-Start.cmd` păstrează gate-ul formal Stage04.
5. Închide doar UI pentru a testa separarea: Core continuă cu ultima configurație. Poți închide, reporni sau termina forțat Core-ul fără oprirea Stage1/2. La o pierdere reală de blocuri, doar abonamentul local intră în fault; pentru reabonare folosește repornirea Core-ului sau reîncărcarea profilului. Nu există replay mascat al cozii.

Verificat pe PC: **30 teste Core/transport PASS**, **12 teste Stage2 PASS**, scenariu real stop/restart/kill Core **PASS**, același UUID și contoare Stage1/2 în avans. Nu s-au deschis DAC-uri în această verificare. Raportul și dovezile sunt în `docs/STAGE010-SOURCE-ISOLATION.md` și `validation/stage010`.

Algoritmii DSP și UI Stage3 sunt neschimbați față de 0.0.9; cele cinci DLL-uri originale Stage2 sunt identice SHA256. Nu este o nouă certificare a tuturor profilelor audio. **Stage04 FAIL REALTIME** și acceptarea formală V07 **BLOCKED** rămân vizibile.

Limita etapei: oprirea Stage2 poate încă afecta sesiunea Stage1. Acest patch izolează lifecycle-ul Core-ului de sursă; independența Stage2 de Stage1 este etapa următoare.

Niciun push sau merge în main pentru această livrare locală.
