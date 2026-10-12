# Matrice format — candidat offline

| Rate intrare | PCM16 stereo | PCM24 valid în 32 stereo | Ieșire |
|---|---|---|---|
| 44100 | PASS offline | PASS offline | FP64/192000 |
| 48000 | PASS offline | PASS offline | FP64/192000 |
| 88200 | PASS offline | PASS offline | FP64/192000 |
| 96000 | PASS offline | PASS offline | FP64/192000 |
| 176400 | PASS offline | PASS offline | FP64/192000 |
| 192000 | PASS offline, exact | PASS offline, exact | FP64/192000, SRC bypass |

PCM24 este MSB-aligned în container signed32; decoderul face shift8 și /8388608. PCM16 /32768. Stereo FL/FR mask3, nBlockAlign și nAvgBytesPerSec exacte, PCM GUID validat. Float32, PCM32 int, packed24, mono, canale/rate suplimentare: refuzate explicit în candidatul actual.

Adaptoarele legacy furnizează FP64 deja convertit la rata sursei, cu original word length din status dacă disponibil. Nu reconstruim formatul brut/GUID/secuencia lipsă. QPC este local de citire; mode UNKNOWN. Nu este probă de raw identity.

WindowsCapture este numai eCapture stereo: cere PCM24-in-32 la rata GetMixFormat; S_OK strict IsFormatSupported/Initialize pentru modul ales, fără closest format și fără fallback. Acesta este formatul efectiv cerut capturii, nu bit depth garantat al fișierului playerului. Shared poate include procesarea Windows. Negocierea/captura reală pentru ambele moduri și fiecare dispozitiv: NOT RUN. Enumerația Windows a fost executată fără pornire de captură.

Driver V2 șase rate × două formate, Shared/Exclusive, byte identity după commit și clock kernel: NOT RUN, neimplementat în această fază.

