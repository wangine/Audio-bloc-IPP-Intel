# Entropy AudioCore 0.1.9 — candidat Headroom CINEMA

Corecție de integrare: detectorul Headroom primește blocul actual post-Matrix atât în MUSIC, cât și în CINEMA, inclusiv crossfade-ul. AUTO, ATTACK/RELEASE, controlul manual și BIT PERFECT sunt păstrate. Algoritmii DSP și UI nu sunt modificați.

**PASS** testul țintit: MUSIC/CINEMA dau aceleași decizii și ieșiri pentru aceleași mostre; MANUAL are multiplicatorul original; BIT PERFECT păstrează payload-ul. **FAIL OVERRANGE** pe fragmentul istoric Stage020 cu MANUAL −3 dB: DAC A pin 1/2, 68 samples într-un bloc, peak maxim post-ASRC 1,047443, marjă observată lipsă 0,403 dB. Nu este o măsurătoare a sesiunii tale live. Nu se modifică profilul/gain-ul automat. Detalii în `docs/STAGE021-HEADROOM-CINEMA-REPORT.md`.

0.1.8 rămâne referința stabilă până la acceptarea audiției 0.1.9. Cele două pachete 0.1.9 sunt separate: capacitate FIFO 32k/64k, aceeași țintă 4096 frames. Nu rula două Core-uri pe același endpoint.

## Probă live explicită

1. Salvează profilul curent, închide UI și oprește Core-ul vechi prin `Stop-AudioCore.cmd` din folderul acelei versiuni. Stage1/2 rămân pornite.
2. Deschide **Live-Test.cmd** dintr-un singur folder 0.1.9 (32k sau 64k). Core-ul nou pornește **DISARMED**, apoi UI se conectează prin IPC.
3. Verifică profilul încărcat, modul MUSIC/CINEMA, Headroom și asignările DAC; armează explicit când ești pregătit pentru audiție.
4. Compară cele două variante cu același profil; pentru trecere, închide UI și oprește doar Core-ul candidat înainte de a porni celălalt folder.

Închiderea UI păstrează Core-ul și starea motorului. Pachetul nu configurează publisherul și nu pornește/oprește Stage1/2. Nu conține profil privat, raw audio, Intel SDK, `.lib` ori directoare build. Intel este legat static; runtime Qt și notificările de licență sunt incluse.

True peak: **NOT IMPLEMENTED**. Stage04: **FAIL REALTIME**, V07: **BLOCKED**, aliniere analogică A/B: nemăsurată. Acest candidat nu rezolvă aceste limite și nu autorizează profilul FIR8193 ×16 pentru live. Pornirea fizică este exclusiv sesiunea de test manual autorizată.
