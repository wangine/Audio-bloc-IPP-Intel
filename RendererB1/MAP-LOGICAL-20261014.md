# B1 — hartă logică a canalelor

Această variantă pentru Git descrie profilul copiat folosit la pregătirea offline din 14 octombrie 2026. ID-urile endpointurilor și numele dispozitivelor rămân în raportul local complet; aici `DAC A/B` sunt doar etichetele logice din profil. Corespondența lor cu amplificatoarele și difuzoarele este **NECONFIRMATĂ**.

`D1`–`D16` sunt destinațiile Output Matrix, numerotate de la 1; `CH1`–`CH16` sunt sursele crossover. DAC A primește lanes 0–7, DAC B lanes 8–15. Launcherul A selectează fișierul PCM A, iar launcherul B selectează PCM B; scriptul de lansare transmite playerului intrarea selectată din manifest fără permutare A/B.

| Launcher | Canal DAC | Destinație / lane | Sursa crossover | Rolul MUSIC | HP | LP | Mute / polaritate |
|---|---:|---|---|---|---|---|---|
| A | 1 | D1 / 0 | CH1 ← Stage2 L | L WAY 1 | ON 80,1 Hz | ON 2040,6 Hz | OFF / normală |
| A | 2 | D2 / 1 | CH5 ← Stage2 R | R WAY 1 | ON 80,1 Hz | ON 2040,6 Hz | OFF / normală |
| A | 3 | D3 / 2 | CH2 ← Stage2 L | L WAY 2 | ON 775,6 Hz | ON 8604,7 Hz | OFF / normală |
| A | 4 | D4 / 3 | CH6 ← Stage2 R | R WAY 2 | ON 775,6 Hz | ON 8604,7 Hz | OFF / normală |
| A | 5 | D5 / 4 | CH3 ← Stage2 L | L WAY 3 | ON 3417,2 Hz | OFF (24101,8 Hz salvat) | OFF / normală |
| A | 6 | D6 / 5 | CH7 ← Stage2 R | R WAY 3 | ON 3417,2 Hz | OFF (24101,8 Hz salvat) | OFF / normală |
| A | 7 | D7 / 6 | CH4 ← Stage2 CX | CENTER WAY 1 | ON 166,7 Hz | ON 15214,4 Hz | OFF / normală |
| A | 8 | D8 / 7 | CH8 ← extracție din Stage2 L+R | SUB 1 | OFF (2,0 Hz salvat) | ON 119,5 Hz | OFF / normală |
| B | 1 | D9 / 8 | CH12 ← aceeași extracție L+R | SCOOP 1 | ON 81,0 Hz | ON 600,4 Hz | OFF / normală |
| B | 2 | D10 / 9 | niciuna | fără cale rutată | — | — | — |
| B | 3 | D11 / 10 | niciuna | fără cale rutată | — | — | — |
| B | 4 | D12 / 11 | niciuna | fără cale rutată | — | — | — |
| B | 5 | D13 / 12 | niciuna | fără cale rutată | — | — | — |
| B | 6 | D14 / 13 | niciuna | fără cale rutată | — | — | — |
| B | 7 | D15 / 14 | niciuna | fără cale rutată | — | — | — |
| B | 8 | D16 / 15 | niciuna | fără cale rutată | — | — | — |

Matricea are `active_source_mask=0x08FF` și `enabled_destination_mask=0xFFFF`. Fiecare dintre D1–D9 are exact o sursă crossover; D10–D16 nu au niciuna. Nu există însumare de surse **în Output Matrix**. În amonte, SUB 1 și SCOOP 1 folosesc sursele Stage2 L și R selectate de `bassFeed=0` și extractorul complex multiband PCA (`bassExtractor=3`); `bassLinkMode=2` reutilizează extracția pentru SCOOP 1. Cele două CH au filtre proprii. O destinație fără semnal de program poate conține dither PCM24, deci „fără cale rutată” nu înseamnă neapărat octeți zero.

Scena salvată este MUSIC: trei căi L/R, o cale CENTER, un SUB și un SCOOP. Niciun CH nu are `solo`, `mute`, `emergencyMute` sau inversare de polaritate activă. CH12 are câștig MUSIC de aproximativ −12,1 dB; celelalte surse rutate au 0 dB. Headroomul manual salvat este −2 dB. Acestea sunt date de profil, nu o identificare a cablării ori un verdict auditiv.
