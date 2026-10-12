# Nivel, reconstrucție și pornirea SRC — 2026-10-10

48 fixture offline prin PCM decoder → SRC real → matrix unitar: șase rate × PCM16/PCM24-in-32 × DC, sinus 1 kHz −1 dBFS, sinus 18 kHz −0.1 dBFS, sinus Fs/4 cu sample peak sub 0 dBFS și vârf între eșantioane. L/R au amplitudini distincte. Date complete: `../evidence/level-results.csv`.

La 192 kHz, ieșirea este identică bit cu bit cu PCM convertit în FP64 pentru toate fixture; copy FP64 verificat și cu signed zero și amplitudini peste 1. Pentru celelalte rate, câștigul în regim stabil este măsurat prin fit sinus/cosinus, independent de fază; DC prin medie. Toleranța de acceptanță este 0.002 dB, eroarea maximă măsurată în aceste fixture aproximativ 7.55×10⁻⁸ dB. Nu reprezintă o caracterizare a întregii benzi/fiecărui fișier audio.

## Exemplul discutat: PCM24, sinus Fs/4, fază π/4

Valorile digitale repetă +0.9192388, +0.9192388, −0.9192388, −0.9192388: sample peak aproximativ −0.73 dBFS. Sinusul periodic care corespunde acestor valori are amplitudinea 1.3, deci vârful autentic aproximativ +2.28 dBFS/dBTP.

| Intrare kHz | Maxim ieșire, inclusiv pornire | Maxim ieșire în regim stabil | Index maxim în ieșirea SRC |
|---|---:|---:|---:|
| 44.1 | +2.34 dBFS | +2.28 dBFS | 11 |
| 48 | +2.37 dBFS | +2.28 dBFS | 10 |
| 88.2 | +2.28 dBFS | +2.28 dBFS | 49 |
| 96 | +2.37 dBFS | +2.28 dBFS | 5 |
| 176.4 | +2.28 dBFS | +2.28 dBFS | 560 |
| 192 | −0.73 dBFS (bypass) | −0.73 dBFS (bypass) | 0 |

La 48/96→192, +2.37 include circa 0.09 dB peste vârful sinusului periodic stabil. Această diferență provine din condiția de pornire: filtrul nu are istoric înaintea începutului abrupt al fixture. Nu este un câștig permanent. Nu afirmăm că orice transient este amplificare artificială: un semnal finit început abrupt diferă de sinusul periodic infinit, iar răspunsul la acea margine depinde de filtrul de reconstrucție și istoric. Algoritmul audio nu a fost schimbat și tranzitoriul nu a fost ascuns cu rampă/limiter.

La 192→192, sample peak în ieșirea digitală rămâne −0.73; estimarea de true peak la intrare rămâne +2.28. Conversia reprezentării FP64 nu înseamnă reconstruirea tuturor vârfurilor între eșantioane.

## Cele trei citiri din Core/Qt

- **Sample Peak:** maxim absolut al eșantioanelor din ultimul pachet decodat, separat L/R. UI arată maximul L/R în dBFS.
- **True Peak (4x estimate):** ramură numai de analiză, r8brain 4× rata de intrare, același profil 2%/217 dB linear phase. Eșantioanele de analiză nu ajung în audio. Prima fereastră de 8192 eșantioane suprasample ale estimatorului este exclusă; în această perioadă UI arată SETTLING. Apoi se raportează maximul estimat al ultimului pachet disponibil, nu un hold cumulativ al întregii piste. Este o estimare de true peak în regim stabil, nu un metru certificat ITU-R BS.1770 și nu garantează detectarea exactă a oricărui transient din fereastra exclusă.
- **SRC Startup Peak:** maximul separat L/R al primelor 8192 eșantioane efectiv produse de SRC la 192 kHz (42.667 ms). Se acumulează, apoi rămâne latched până la format/generation/discontinuity nouă. Este nivelul total din fereastra de pornire, nu diferența de gain atribuită filtrului. La 192 kHz este doar observație de bypass, fără filtrare SRC.

În CSV, output_peak include întreaga ieșire disponibilă; steady_peak exclude câte 8192 samples din capete; true_peak_estimate și src_startup_peak provin din meter-ul integrat. Startup metering nu elimină/taie acele eșantioane din sunet.

Distincția sample/true peak este descrisă în [ITU-R BS.1770, Annex 2](https://www.itu.int/dms_pubrec/itu-r/rec/bs/r-rec-bs.1770-2-201103-s%21%21pdf-e.pdf); implementarea analizei folosește [r8brain-free-src](https://github.com/avaneev/r8brain-free-src), nu filtrul de conformitate al acelei recomandări.

Două surse active se însumează și pot trece de 0 dBFS. FP64 permite păstrarea vârfurilor fără clip; un gain unitar nu garantează că sample peak de după SRC/sumă rămâne sub 0 dBFS.

