param([ValidatePattern('^[a-z0-9-]+$')][string]$Version='stage1-native-music-v0002')
$ErrorActionPreference='Stop'
$stage1ProjectRoot=Split-Path $PSScriptRoot -Parent
$stage1ReleaseRoot=Join-Path $stage1ProjectRoot 'release'
$stage1RuntimeRoot=Join-Path $stage1ReleaseRoot $Version
$stage1SourceRoot=Join-Path $stage1ReleaseRoot "$Version-source"
if((Test-Path -LiteralPath $stage1RuntimeRoot) -or (Test-Path -LiteralPath $stage1SourceRoot)){throw 'Use a fresh version directory; existing delivery is preserved'}
$stage1Expected=@{
 'EntropyAuxEndpoint.dll'='F934B7BFB4E326F3451B6CB0D9C925750173E7A57A5A2A046E53374EA472985A'
 'EntropyMusicTransportEndpoint.dll'='FDD62779F0B2E7A3F7278C17CBE33F29F156DD167E9F098CE239B398039DEC0B'
 'EntropyTidalEndpoint.dll'='4D47B47E74547EA6E9E8C391972EAB1DFBCF46B50082266171B82B7CB4B6267B'
}
foreach($stage1Name in $stage1Expected.Keys){if((Get-FileHash -LiteralPath "$stage1ProjectRoot\reference-runtime\$stage1Name").Hash -ne $stage1Expected[$stage1Name]){throw "Reference hash mismatch: $stage1Name"}}
foreach($stage1Required in @('build\EntropyMusicSourceCore.exe','build-qt\EntropyMusicSourceQt.exe','evidence\level-results.csv','evidence\qt-smoke.png')){if(!(Test-Path -LiteralPath (Join-Path $stage1ProjectRoot $stage1Required))){throw "Missing verified artifact: $stage1Required"}}
foreach($stage1Folder in @('core','ui','state','docs','evidence','licenses')){New-Item -ItemType Directory -Path (Join-Path $stage1RuntimeRoot $stage1Folder) -Force | Out-Null}
Copy-Item -LiteralPath "$stage1ProjectRoot\build\EntropyMusicSourceCore.exe" -Destination "$stage1RuntimeRoot\core"
Copy-Item -LiteralPath "$stage1ProjectRoot\build\providers" -Destination "$stage1RuntimeRoot\core" -Recurse
foreach($stage1Name in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')){Copy-Item -LiteralPath "$env:SystemRoot\System32\$stage1Name" -Destination "$stage1RuntimeRoot\core"}
Copy-Item -LiteralPath "$stage1ProjectRoot\build-qt\EntropyMusicSourceQt.exe" -Destination "$stage1RuntimeRoot\ui"
$env:PATH='C:\Qt\6.11.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;'+$env:PATH
& 'C:\Qt\6.11.2\mingw_64\bin\windeployqt.exe' --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$stage1RuntimeRoot\ui\EntropyMusicSourceQt.exe" *> "$stage1ProjectRoot\evidence\windeployqt.txt"
if($LASTEXITCODE){Get-Content "$stage1ProjectRoot\evidence\windeployqt.txt" -Tail 15;throw 'Qt deployment failed'}
Copy-Item -LiteralPath 'C:\Qt\6.11.2\mingw_64\plugins\platforms\qoffscreen.dll' -Destination "$stage1RuntimeRoot\ui\platforms"
Copy-Item -LiteralPath "$stage1ProjectRoot\README.md" -Destination $stage1RuntimeRoot
Get-ChildItem -LiteralPath "$stage1ProjectRoot\docs" -File | Copy-Item -Destination "$stage1RuntimeRoot\docs"
foreach($stage1Name in @('Stage1-Launch.ps1','Start-Core.ps1','Start-UI.ps1','Stop-Core.ps1','Start-Stage1-Music.cmd','ColdStart-Stage1-Music.cmd','Stop-Stage1-Music.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $stage1Name) -Destination $stage1RuntimeRoot}
@('[music]','providerId=','endpointId=','mode=8','[aux]','providerId=','endpointId=','mode=4') | Set-Content -LiteralPath "$stage1RuntimeRoot\state\Stage1Music.ini" -Encoding ascii
foreach($stage1Name in @('level-results.csv','qt-smoke.png','qt-smoke.png.json','progress.md','build.txt','build-qt.txt','live-continuity-35s.json','live-continuity-summary.json','live-after-backpressure.json','live-fault-before-backpressure.json','packaged-live-fix-ctest.log')){Copy-Item -LiteralPath "$stage1ProjectRoot\evidence\$stage1Name" -Destination "$stage1RuntimeRoot\evidence"}
Copy-Item -LiteralPath "$stage1ProjectRoot\ThirdParty\NativeSrc\Modules\SRC\R8Brain\src\r8brain-free-src\LICENSE" -Destination "$stage1RuntimeRoot\licenses\r8brain-MIT.txt"
Copy-Item -LiteralPath 'C:\Qt\Licenses' -Destination "$stage1RuntimeRoot\licenses\Qt" -Recurse
$stage1Notice=@'
Local candidate, not a public release. Native engine sources in the paired source archive.
r8brain-free-src: Aleksey Vaneev, MIT; original LICENSE copied here and in ThirdParty.
Qt 6.11.2 Core/Gui/Widgets: dynamic client-only libraries from local C:/Qt distribution.
Qt licensing text copied from that distribution; Qt source: https://download.qt.io/archive/qt/6.11/6.11.2/single/
MinGW runtime dependencies are kept separate from the MSVC engine and legacy reference modules.
Reference ingress DLLs: Audio-bloc-IPP-Intel local release, SHA matched to stage1-passive-v0002.
No Matrix/SRC legacy DLL is loaded by the native core. No driver installer in this package.
Frozen StageBus MUSIC SDK/StagePublish files originate from the separately verified Stage2 MUSIC workspace.
'@
Set-Content -LiteralPath "$stage1RuntimeRoot\licenses\NOTICE.txt" -Value $stage1Notice -Encoding utf8
$stage1Manifest=[ordered]@{
 version=$Version;date='2026-10-10';phase='B native Core + separate Qt, offline candidate';native_src_tag='stage1-passive-v0002';native_src_commit='48f979f489264885ab1a16351f81c43ddf64b0f1'
 compiler='MSVC 19.51.36257 x64';ui='Qt 6.11.2 GNU 13.1.0 separate process';music_layout=3;output_channels=2;output_rate=192000;block_frames=1024
 contract_hash='8d1dd0d0b733489a83ebb5a84ed287ad211175d27d7fbea64fa969130f55318e';mute=$false;gain='unity, single-source exact copy / unrestricted dual-source sum'
 meters=@('input sample peak','4x true peak estimate with settling status','first 8192 output frames SRC startup peak');reference_ingress_hashes=$stage1Expected
 runtime_vcrt='System32 copies matching the offline-tested native process dependencies';driver_v2='NOT IMPLEMENTED / NOT INSTALLED';live_capture='TIDAL legacy Music Transport 16/44.1 observed 35 s; no new losses, faults, overflows or steady underruns; DAC not tested';stage2_interop='actual packaged MUSIC v0009 MSVC->MinGW PASS';source_archive="$Version-source.zip"
}
$stage1Manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$stage1RuntimeRoot\manifest.json" -Encoding utf8
New-Item -ItemType Directory -Path $stage1SourceRoot -Force | Out-Null
foreach($stage1Name in @('ABI','Engine','Providers','ThirdParty','UI','tools','tests','reference-runtime','docs','CMakeLists.txt','README.md')){Copy-Item -LiteralPath (Join-Path $stage1ProjectRoot $stage1Name) -Destination $stage1SourceRoot -Recurse}
New-Item -ItemType Directory -Path "$stage1SourceRoot\evidence" -Force | Out-Null
foreach($stage1Name in @('progress.md','level-results.csv','qt-smoke.png','qt-smoke.png.json','live-continuity-35s.json','live-continuity-summary.json','live-after-backpressure.json','live-fault-before-backpressure.json','packaged-live-fix-ctest.log')){Copy-Item -LiteralPath "$stage1ProjectRoot\evidence\$stage1Name" -Destination "$stage1SourceRoot\evidence"}
foreach($stage1Name in @('instruction(20261010-180432).md','STAGE1-PHASE-A.md','STAGE1-PHASE-A-FREEZE.json')){Copy-Item -LiteralPath (Join-Path "$stage1ProjectRoot\..\docs\stage1" $stage1Name) -Destination "$stage1SourceRoot\docs"}
Copy-Item -LiteralPath "$stage1RuntimeRoot\manifest.json" -Destination $stage1SourceRoot
foreach($stage1Delivery in @($stage1RuntimeRoot,$stage1SourceRoot)){
 $stage1Lines=Get-ChildItem -LiteralPath $stage1Delivery -Recurse -File | Where-Object { [IO.Path]::GetRelativePath($stage1Delivery,$_.FullName) -notmatch '^state[/\\]' } | Sort-Object FullName | ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName).Hash,[IO.Path]::GetRelativePath($stage1Delivery,$_.FullName).Replace('\','/')}
 $stage1Lines | Set-Content -LiteralPath "$stage1Delivery\SHA256SUMS.txt" -Encoding ascii
}
Compress-Archive -LiteralPath $stage1RuntimeRoot -DestinationPath "$stage1ReleaseRoot\$Version-runtime.zip" -CompressionLevel Optimal
Compress-Archive -LiteralPath $stage1SourceRoot -DestinationPath "$stage1ReleaseRoot\$Version-source.zip" -CompressionLevel Optimal
@("$stage1ReleaseRoot\$Version-runtime.zip","$stage1ReleaseRoot\$Version-source.zip") | ForEach-Object {'{0}  {1}' -f (Get-FileHash -LiteralPath $_).Hash,[IO.Path]::GetFileName($_)} | Set-Content -LiteralPath "$stage1ReleaseRoot\$Version-SHA256SUMS.txt" -Encoding ascii
Write-Output "Packaged $stage1RuntimeRoot — empty selectors, no production activation."
