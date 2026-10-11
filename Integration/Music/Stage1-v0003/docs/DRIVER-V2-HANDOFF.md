# Driver V2 — proiect separat după Core

Proiectul Stage1 user-mode este pregătit pentru pluginuri `endpoint_provider_v2.h`. Un eRender virtual selectabil de TIDAL/Spotify nu poate fi înregistrat numai prin WASAPI; este necesar un driver Windows. Nu s-a construit/instalat/activat niciun driver aici.

Identitate nouă `Entropy Music Render V2`, distinctă de driverele existente. Stereo PCM16 și PCM24 valid în 32 MSB-aligned la 44.1/48/88.2/96/176.4/192 kHz. Shared/Exclusive cu negocieri exacte, format nesuportat refuzat. Fără SRC/gain/dither/limiter/Qt/DAC real/ASIO/MME/client Direct KS. WaveRT/PortCls/KS intern Windows rămâne infrastructura necesară.

Clock per stream: QPC start/frequency reale, rată și block align negociate, contor monotonic uint64 frames, remainder fracționar păstrat. DMA modulo nu schimbă poziția logică; notificarea, packet index și commit sunt derivate din același model. Nicio rotunjire cumulativă la milisecunde.

Bridge bounded cu un singur ownership/commit path: numai pachete complet scrise, un singur publish pentru commit, fără citire concurentă cu rescrierea bufferului. Nu combina SetWritePacket și UpdatePosition ca producători simultani. Ring per stream, sequence/first frame/generation/QPC/format atomic și contori de pierdere; lipsa Stage1/Qt/Stage2 nu oprește driverul și nu acumulează memorie nelimitată.

Adaptorul V2, proiect separat de Core, trebuie să transmită bytes PCM originale și metadate fără fabricare: original word length = payload PCM; packet_sequence nenul, firstSourceFrame, generation nouă la restart/format, lostFrames cumulativ, discontinuity explicit. Pluginul adaptează bridge-ul real prin C ABI, nu un client KS audio moștenit.

Înainte de import: alege și pin-uiește commitul Microsoft Windows-driver-samples și fișierele folosite; verifică licențele, WDK/SDK real (10.0.28000.0 inventariat), build, signing și INF. Nu importa un sample nepin-uit. Teste focalizate: 12 formate × Shared/Exclusive, PCM bytes commit→adaptor pentru două pachete consecutive LR distincte, wrap/duplicate/gap/clock position, pornire fără Qt/Stage1/Stage2, stop/restart/generation. Măsurarea True Peak rămâne în Core, nu în driver.

Instalarea/activarea viitorului driver necesită aprobarea explicită a utilizatorului. Etapa curentă livrează handoff și ABI, nu implementare kernel ori garanție Exclusive.

