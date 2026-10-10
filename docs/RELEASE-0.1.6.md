# Entropy AudioCore 0.1.6 — audit01 candidate, live acceptance pending
Cauză: calificarea sintetică sincronă bloca handler-ele ARM/MUSIC/CINEMA și întârzia DISARM.
Corecție: LifecycleExecutor, CoreRuntime, CommandServer, cache pregătire Crossover, DeviceService și backend IPC EngineFacade.
Build: PASS MSVC 19.51.36257 x64 / Intel IPP 2026.0; UI Qt 6.11.2 / MinGW 13.1.
Regresii: PASS 4 native + UI, transport IPC existent, smoke Core/UI și integritate surse; fără suită completă/benchmark.
Simulare: DISARM admitere 4,7 us; gate 0,6 us; teardown 0,068 ms; cu reply mare necitit admitere 15 ms.
Pregătire lentă: admitere 1,0 us, gate 0,2 us, zero porniri tardive; contor benchmark zero. Timpii driverului nu sunt garantați.
Numeric cache: 307200 samples, toate cele 16 ieșiri, bit-exact, diferență maximă 0; funcții DSP originale neschimbate.
Hardware/listening: NOT RUN/PENDING; Stage04 FAIL REALTIME și V07 BLOCKED rămân consemnate.
Pornire: închide UI stabil, oprește Core stabil prin Stop-AudioCore.cmd, apoi pornește Live-Test.cmd din acest folder; ARM rămâne explicit. Stage1/2 rămân pornite.
Core SHA256: A7B1AF6C7B00E768311138C5B0997D864668194DCDB0B89EFD08BD8C703369CB; commit exact în manifest.json.
Auditul original este în folderul de lucru; se atașează release-ului după confirmarea live. Detalii: docs/CONTROL-LIFECYCLE-0.1.6.md și validation/stage017.
