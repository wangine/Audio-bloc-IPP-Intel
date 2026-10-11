# Music Render V2 0.3.0.11 — pachet DEV păstrat

Arhiva conține SYS/INF/CAT semnate cu certificatul DEV existent, providerele raw,
auditul PCM/FP64, runnerul, sursele pin-uite și inventarul SHA256. Nu conține chei
private sau modificări ale politicii de boot. Instalarea acestei sesiuni a actualizat
numai `ROOT\MEDIA\0003` / `ROOT\EntropyMusicRenderV2`; versiunea este 0.3.0.11.

ZIP SHA256: `7C96272894FF8A64725E3521356C95733E76FD8FE94722168A2179407D79EB3C`.
SYS semnat SHA256: `FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA`.

[Raportul încărcat și limitele](../../../../validation/music-render-v2/LOADED-DRIVER-0.3.0.11.md)
consemnează 12 formate raw și 23.040.000 cadre native FP64 pe Stage1 v0003-r3.
Textele DEPLOYMENT/README din interior includ contextul istoric 0.3.0.10; sursa
și binarul kernelului acestui pachet sunt 0.3.0.11, cu identitate și hash explicite.

`PackageTests.py` din arhiva driverului este suita **înainte de semnare**: cere
NotSigned și compară SYS cu build-ul nesemnat. Nu se aplică direct livrării semnate.
Pachetul semnat se verifică prin inventarul manifestului, SignTool/AuthentiCode,
apartenența SYS la catalog și hashul driverului încărcat. Verificarea alterării unui
artifact rămâne activă în `tools/Assert-Package.ps1` și runnerul livrat.

Pentru Core-ul nou utilizați runnerul cu Core/SHA explicit din
[Stage1v0003](../../../Music/Stage1-v0003/tools/Run-Diagnostic-Candidate.ps1).
Runnerul driverului păstrează separat referința înghețată v0002; nu îi transformă
eșecurile istorice în rezultate trecute. Polling rămâne refuzat.
