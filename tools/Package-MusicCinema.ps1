param([ValidateSet('0.1.9-v2')][string]$Version='0.1.9-v2',[string]$OutputRoot='D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release',[string]$UiBuild='build/qt-music-cinema')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path;$destination=Join-Path $OutputRoot $Version
if(Test-Path -LiteralPath $destination){throw 'Preserve existing v2 distribution; packaging destination must be new'}
$report=Join-Path $root 'docs/VALIDATION-MUSIC-CINEMA-v2.json';if(!(Test-Path -LiteralPath $report)){throw 'Verified validation report required'}
$protected=Get-Content -LiteralPath (Join-Path $root 'validation/music-cinema-v2/protected-baseline.json') -Raw|ConvertFrom-Json
foreach($file in $protected){if((Get-FileHash -LiteralPath (Join-Path $root $file.path)).Hash -ne $file.sha256){throw "Protected source changed: $($file.path)"}}
New-Item -ItemType Directory -Path $destination|Out-Null
foreach($variant in @(@{name='32k';frames=32768},@{name='64k';frames=65536})){
 $bundle=Join-Path $destination "EntropyAudioCore-$Version-$($variant.name)-win-x64";$runtime=Join-Path $bundle 'runtime';$tools=Join-Path $bundle 'tools'
 New-Item -ItemType Directory -Path $runtime,$tools|Out-Null
 Copy-Item -LiteralPath (Join-Path $root "build/msvc-music-cinema-$($variant.frames)/EntropyAudioCore.exe") -Destination $runtime
 Copy-Item -LiteralPath (Join-Path $root "$UiBuild/EntropyUI.exe") -Destination $runtime
 $env:PATH='C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;'+$env:PATH
 & 'C:/Qt/6.11.2/mingw_64/bin/windeployqt.exe' --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-ffmpeg --compiler-runtime (Join-Path $runtime 'EntropyUI.exe') *> (Join-Path $destination "deploy-$($variant.name).txt")
 if($LASTEXITCODE){throw 'Qt deployment failed'}
 Copy-Item -LiteralPath 'C:/Qt/6.11.2/mingw_64/plugins/platforms/qoffscreen.dll' -Destination (Join-Path $runtime 'platforms')
 Copy-Item -LiteralPath (Join-Path $root 'tools/Start-MusicCinema.ps1'),(Join-Path $root 'tools/Stop-MusicCinema.ps1') -Destination $tools
 Set-Content -LiteralPath (Join-Path $bundle 'Cold-Start.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-MusicCinema.ps1" %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Encoding ascii -Value @('@echo off','call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*','exit /b %ERRORLEVEL%')
 Set-Content -LiteralPath (Join-Path $bundle 'Stop-AudioCore.cmd') -Encoding ascii -Value @('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Stop-MusicCinema.ps1" %*','exit /b %ERRORLEVEL%')
 Copy-Item -LiteralPath (Join-Path $root 'docs/MUSIC-CINEMA-v2.md') -Destination (Join-Path $bundle 'README.md');Copy-Item -LiteralPath $report -Destination $bundle
 Copy-Item -LiteralPath 'D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release/0.1.8/EntropyAudioCore-0.1.8-32k-win-x64/notices' -Destination $bundle -Recurse
 [ordered]@{version=$Version;variant=$variant.name}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tools 'Start-MusicCinema.ps1') -SmokeTest *> (Join-Path $destination "smoke-$($variant.name).txt")
 if($LASTEXITCODE){throw 'Packaged Core/IPC/Qt smoke failed; see smoke log'}
 $validated=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json;$validated|Add-Member -NotePropertyName packagedVariant -NotePropertyValue $variant.name;$validated|Add-Member -NotePropertyName packagedSmoke -NotePropertyValue 'PASS';$validated|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $bundle 'VALIDATION-MUSIC-CINEMA-v2.json') -Encoding utf8
 $files=@(Get-ChildItem -LiteralPath $bundle -Recurse -File|Where-Object {$_.Name -ne 'manifest.json' -and $_.FullName -notlike "$bundle\logs\*"}|Sort-Object FullName|ForEach-Object {[ordered]@{path=$_.FullName.Substring($bundle.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 $manifest=[ordered]@{version=$Version;variant=$variant.name;fifoCapacityFrames=$variant.frames;targetFrames=4096;architecture='Windows x64 native Core; separate Qt IPC UI';sourceBaseCommit='2f3c45aa71cbf1bc352aece848060dc7523b0649';sourceModifiedLocal=$true;automaticArm=$false;music='3ch FP64 planar/192000/1024';cinema='6/8ch independent; real Decoder PENDING';stage04='FAIL REALTIME; inherited offline test failure retained';v07='BLOCKED';files=$files}
 $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 $zip=Join-Path $destination "EntropyAudioCore-$Version-$($variant.name)-win-x64.zip"
 # Archive verified payload only: smoke logs/captures and private runtime state stay local.
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
 try {foreach($relative in @($files.path)+@('manifest.json')){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $bundle $relative),([IO.Path]::GetFileName($bundle)+'/'+$relative),[IO.Compression.CompressionLevel]::Optimal)|Out-Null}}finally{$archive.Dispose()}
 Add-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.txt') -Encoding ascii -Value ((Get-FileHash -LiteralPath $zip).Hash+'  '+[IO.Path]::GetFileName($zip))
 Write-Output $bundle
}
$source=Join-Path $destination "EntropyAudioCore-$Version-source.zip"
& 'C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe' (Join-Path $root 'tools/Package-MusicCinemaSource.py') $root $source 'D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/local/stage04-ref/Stage3/native-reference' $Version
if($LASTEXITCODE){throw 'Source archive failed'}
Add-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.txt') -Encoding ascii -Value ((Get-FileHash -LiteralPath $source).Hash+'  '+[IO.Path]::GetFileName($source))
