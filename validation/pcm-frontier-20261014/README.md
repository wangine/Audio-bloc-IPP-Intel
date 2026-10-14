# Prima frontieră PCM — 14 octombrie 2026

Raportul integral al investigației Tidal 24/96 este în [FRONTIER-REPORT.md](FRONTIER-REPORT.md). Datele derivate sunt în `analysis.json`, `reference-comparison.json` și `first-bit-difference-window.json`. `manifest.json` conține identitățile SHA256 și dimensiunile artefactelor originale. Scripturile de analiză și sursa referinței offline sunt incluse pentru inspectarea metodei.

Cele 12 fișiere ale investigației au fost copiate aici fără modificări din directorul local `pcm-frontier-20261014`; hashurile lor au fost comparate înainte de commit. Căile absolute din raport, manifest și scripturi descriu mediul în care s-a făcut măsurarea. În special, `offline_ref` depinde de sursa Stage1 r7 aflată în acel mediu și nu este un proiect autonom.

Capturile `stage1-fp64.bin` și `stage2-fp64.bin`, referința `offline-r8brain-fp64.bin`, fișierul WAV și executabilul compilat nu sunt în Git. Hashurile lor rămân în manifest. Prin urmare, analiza nu poate fi recalculată doar din acest checkout. Fișierele JSON publicate conțin metadate derivate ale sesiunii, inclusiv identificatori și tickuri QPC. Ultima propoziție din raport consemnează starea încărcării la momentul redactării lui; această publicare ulterioară adaugă raportul și metadatele derivate, fără capturile brute.
