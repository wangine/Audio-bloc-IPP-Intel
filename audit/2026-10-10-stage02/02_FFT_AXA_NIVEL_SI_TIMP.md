# Etapa 02 — FFT și răspuns real, pe aceeași axă

**Scop:** corectează afișajul și elimină munca FFT/response din cererile obișnuite de control. Nu modifica transferul audio pentru a-l potrivi cu graficul greșit.

## Defecte confirmate

Core generează response[256] pe 20…60000 Hz; UI reetichetează aceiași indici pe 20…5000/40000 Hz. Hann este normalizat deja, dar FFT este scalat cu 4/N. Decay este 1,5 dB per apel, deci depinde de ritmul telemetriei. Snapshot-ul controlului calculează și view-ul.

## Execuție

1. Furnizează o grilă explicită `responseFrequencyHz` împreună cu punctele răspunsului. UI proiectează fiecare frecvență pe axa locală și taie vizual în afara domeniului. Alternativ, resamplează matematic grila sursă la grila locală înainte de desen; este interzisă simpla schimbare a etichetei pentru același index. Extinde contractul IPC cu câmpuri versionate și păstrează protecția față de un client vechi/incompatibil.
2. Calculează răspunsul din configurația APLICATĂ, nu din valori UI încă pending: produsul secțiunilor biquad efective; kernelul real pentru FIR; gain-ul de canal și efectul întârzierii fracționare alese. Limitele per rol și bypass-ul LFE trebuie respectate. All-pass/întârzierea ideală au modul unitar, dar modelul complex trebuie păstrat pentru analiză. În tranziție, etichetează starea; nu pretinde un singur transfer staționar pentru un crossfade variabil.
3. Compilează/cache-uiește răspunsul în worker-ul de vizualizare la schimbarea generației DSP. Nu reconstrui toate curbele la fiecare 150 ms. Nu pune funcții FFT, lookup-uri masive de parametri ori alocări în audio callback.
4. Păstrează Hann normalizat la sumă N și schimbă scala FFT în `2/N` pentru binurile interioare, `1/N` pentru DC/Nyquist. Nu compensa încă o dată fereastra. Declară explicit unitatea dBFS peak pentru sinus; nu amesteca nivelul unui bin, RMS integrat și densitatea de zgomot.
5. Schimbă decay-ul în funcție de timp: inițial 60 dB/sec în afara unei eventuale preferințe existente, calculat cu timestamp monotonic între cadre audio noi. Același cadru nu trebuie să sufere decay suplimentar doar pentru că un al doilea client îl citește. Păstrează N=16384 pentru această etapă; nu pretinde rezoluție LF suplimentară prin cosmetizarea axei.
6. Publică cadre imuabile/coerente cu `channel`, `program`, `generation`, `frameSequence`, `captureQpc`, `sampleRate`, N și puncte. Refolosește storage prealocat. Audio doar livrează mostrele monitorizate; worker-ul face FFT; controlul doar citește ultimul cadru complet. Nu calcula FFT în `CrossoverAccess::snapshot()` și nu citi neprotejat vectorii mutabili ai filtrului.
7. La schimbare de canal/scenă, invalidează eticheta disponibilă până există o fereastră din noua generație. O cerere idempotentă pentru același monitor nu resetează smoothing-ul. Dacă nu există un cadru nou de peste 500 ms, marchează STALE/unavailable prin datele existente, nu menține o curbă veche ca live. Păstrează aspectul UI.

## Verificare minimă

Un singur target offline, trei cazuri: (a) grilă cu un reper 119,5 Hz rămâne 119,5 Hz în panoul 5k; (b) sinus centrat în bin la 1500 Hz, peak0,125, este −18,0618 dBFS ±0,05 dB; (c) aceeași scădere temporală are aceeași evoluție când UI citește la 10 sau 20 Hz și niciun cadru vechi nu este etichetat drept alt canal. Nu rula suita audio completă pentru o eroare de desen.

O verificare vizuală scurtă pe LR/Center/Bass confirmă doar afișajul. Un crop/resampling de grafic nu este o dovadă despre calitatea analogică.


## Surse inspectate

- [`DSP/Crossover/compat/src/CrossoverModule.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/compat/src/CrossoverModule.cpp)
- [`DSP/Crossover/include/IppSpectrum.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/include/IppSpectrum.h)
- [`DSP/Crossover/include/RealtimeFir.h`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/include/RealtimeFir.h)
- [`DSP/Crossover/src/PluginProcessor.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/DSP/Crossover/src/PluginProcessor.cpp)
- [`Engine/Control/CrossoverAccess.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/Engine/Control/CrossoverAccess.cpp)
- [`UI/Client/EngineFacade.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Client/EngineFacade.cpp)
- [`UI/Legacy/R10/CrossoverPage.cpp`](https://github.com/wangine/Audio-bloc-IPP-Intel/blob/3afb8d77b3bdd984a2f5bb53095cb99c37de72c0/UI/Legacy/R10/CrossoverPage.cpp)
