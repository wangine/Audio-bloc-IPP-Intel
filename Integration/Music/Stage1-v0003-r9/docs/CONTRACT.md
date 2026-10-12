# Contract candidat Stage1 MUSIC

Ieșire pasivă StagePublish V2, `MUSIC_STEREO2_V1` ID 3, L/R FP64 LE planar, 192000 Hz, 1024 frames. Stage2 MUSIC v0009 consumă două canale și publică `MUSIC_LRC3_V1` ID 4. Contract hash `8d1dd0d0b733489a83ebb5a84ed287ad211175d27d7fbea64fa969130f55318e`. Header session 256 bytes; block 128 bytes. Hashurile/layouturile sunt cele copiate exact din SDK MUSIC.

Session UUID este nenul și constant până la oprirea Core. Sequence începe la 1, first_frame = (sequence−1)×1024; epoch 1; QPC_frequency real. Validity/discontinuity au numai mask 3 MUSIC/AUX; elementul source[2] rămâne zero. Publicarea continuă fără consumator. Queue per subscriber 16 blocuri; overflow este GAP explicit și impune reconectare, nu reset al producătorului.

Ingress `endpoint_provider_v2.h`: C ABI 0x00020000, format 64 bytes, packet 160 bytes, provider API 336 bytes. Pluginurile dețin handleurile; cancel/join/close preced unload. Raw PCM este interleaved L/R; FP64 legacy este planar L/R și METADATA_PARTIAL, fără raw identity fabricată. OriginalContainerBits/OriginalValidBits sunt distincte de formatul payloadului. Zero/zero înseamnă unknown. QPC pentru legacy și captură este timestamp de citire/publicare al adaptorului, nu dovadă de commit kernel.

Core nu cunoaște ID-urile/numele DLL ale providerilor; acestea sunt în adaptoare și profil. Nu deschide DAC și nu trimite comenzi Stage2/3. Un ingress MUSIC activ. AUX independent. Same endpoint/provider dublu refuzat; detectarea aceluiași stream fizic prin două adaptoare diferite nu este garantată de metadatele V1.

Control `stage1_control_v1.h`: magic 0x3143544D31535445, ABI 0x00010000, request 680 bytes, source snapshot 880 bytes, response sub 64 KiB. STATUS=0, ENUMERATE=1, SELECT=2; nu există operație mute. Pipe local cu ACL pentru SID-ul utilizatorului și remote clients respinși. Operațiile invalide/version/size/reserved nonzero nu modifică selecțiile. Transfer incomplet este abandonat cu timeout finit.

ENUMERATE paginează 32 intrări per răspuns prin catalogOffset ≤4096; offset 0 reface catalogul, truncated indică pagini următoare, catalogTotal numărul total. Qt colectează toate paginile. Catalogul poate necesita refresh dacă Windows schimbă lista în timpul enumerării.

Scheduler și fiecare worker au ownership unic. Prepare/SRC/meter setup pot aloca pe worker; decodarea/SRC/măsurarea/push/pop/matrix utilizează stocare pregătită. FIFO SPSC 16×1024 frames (85.333 ms la 192 kHz), fără tuning automat. Lipsa unui bloc produce sursă invalidă și zero în acel bloc. Workerul de intrare păstrează blocul produs și așteaptă spațiu în FIFO, cu verificarea anulării la fiecare milisecundă; schedulerul audio nu așteaptă. Nu se pierd mostre pentru a absorbi backlogul. Apelantul direct fără callback de așteptare primește în continuare fault și contor explicit la overflow. Nu se inserează/șterge eșantioane pentru drift correction. Format/generation/discontinuity invalidează blocurile vechi inclusiv un bloc incomplet.

În r8, clientul Qt permite SELECT numai pentru MUSIC și păstrează comanda până când tranzacția IPC aflată în curs se termină. AUX este legat prin profil și inițializarea launcherului la `legacy.aux` / `entropy-aux:system-default` / mode4, fără controale Qt de selecție. Protocolul de control generic rămâne neschimbat.

Adaptorul AUX acceptă V1 original fără catalog. Citirea privată nonblocking furnizează L/R FP64; dreptul de acces real se transmite numai dacă flagurile originale confirmă Shared sau Exclusive. Absența lor rămâne UNKNOWN. Un bloc gol transmite IDLE/RUN din statusul bridge-ului, fără PCM inventat. DLL-ul original nu furnizează secvența brută sau indicele primului cadru; nu este raw-PCM certified.

R8brain neschimbat: 2% transition band, 217 dB, linear phase. Finite FP64 overrange se păstrează; NaN/Inf → zero după politica referinței. La 192 kHz se ocolește filtrarea SRC. Selecția unei singure surse face copy exact, inclusiv signed zero; două surse se adună fără normalizare.

