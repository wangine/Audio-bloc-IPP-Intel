# Renderer B1

B1 este candidatul pentru redarea unui fișier finit, pregătit offline din captura Stage2, către **un singur DAC la un moment dat**. Nu este rendererul live cu două ceasuri și nu constituie un verdict de calitate audio.

## Documente și verificări

- [DESIGN.md](DESIGN.md): arhitectura, invariabilele și limitele B1.
- [CODEX.md](CODEX.md): instrucțiunile pentru buildul și pregătirea locală inițială.
- [Tests.cpp](Tests.cpp): testul punctual al împachetării, cursorului și headerului PCM. [VALIDATION-20261014.md](VALIDATION-20261014.md) consemnează rezultatul rulat local.
- [MAP-LOGICAL-20261014.md](MAP-LOGICAL-20261014.md): traseul canalelor prin Output Matrix și crossover, fără identități de endpoint sau presupuneri despre cablarea fizică.
- [Raportul Stage A](../validation/pcm-frontier-20261014/FRONTIER-REPORT.md): ce a fost verificat înainte de intrarea în renderer.

`Build-B1.ps1` construiește numai executabilele B1, rulează testul punctual și pregătește offline PCM dintr-o captură și un profil validate. Scriptul cere o destinație nouă; o destinație existentă nu se suprascrie. Fiecare pachet local are manifest, hashuri și launchere proprii. Fișierele PCM, profilul salvat și ID-urile endpointurilor rămân locale.

Un rezultat pozitiv al testului de pachete sau al pregătirii offline nu validează WASAPI, ordinea mufelor fizice, amplificatoarele ori sunetul perceput. Pachetul inspectat în raportul de validare nu a fost încărcat în Git.
