param(
    [Parameter(Mandatory=$true)][string]$Version,
    [ValidateSet('beta test','stable beta')][string]$Channel='beta test',
    [string]$OutputRoot='release',
    [string]$NativeBuild='build/msvc-pcm24-v2',
    [string]$UiBuild='build/qt-stage07',
    [string]$QtRoot='C:/Qt/6.11.2/mingw_64',
    [string]$QtDocs='C:/Qt/Docs/Qt-6.11.2',
    [string]$IppRoot='C:/Program Files (x86)/Intel/oneAPI/ipp/2026.0',
    [string]$MingwRoot='C:/Qt/Tools/mingw1310_64'
)
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid release version'}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output=Join-Path (Join-Path $root $OutputRoot) $Version
if(Test-Path -LiteralPath $output){throw "Version folder already exists: $output. Preserve it and choose a new version."}
$bundle=Join-Path $output ('EntropyAudioCore-'+$Version+'-beta-win-x64')
$runtime=Join-Path $bundle 'runtime'
New-Item -ItemType Directory -Path $runtime,(Join-Path $bundle 'tools'),(Join-Path $bundle 'notices') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root "$NativeBuild/EntropyAudioCore.exe") -Destination $runtime
Copy-Item -LiteralPath (Join-Path $root "$UiBuild/EntropyUI.exe") -Destination $runtime
$env:PATH="$QtRoot/bin;$MingwRoot/bin;"+$env:PATH
& "$QtRoot/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-ffmpeg --compiler-runtime (Join-Path $runtime 'EntropyUI.exe') *> (Join-Path $output 'windeployqt.txt')
if($LASTEXITCODE){throw "Qt deployment failed; see $output"}
Copy-Item -LiteralPath (Join-Path $QtRoot 'plugins/platforms/qoffscreen.dll') -Destination (Join-Path $runtime 'platforms')
Copy-Item -LiteralPath (Join-Path $root 'tools/Start-Beta.ps1'),(Join-Path $root 'tools/Stop-AudioCore.ps1') -Destination (Join-Path $bundle 'tools')
$cold=@('@echo off','powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Start-Beta.ps1" %*','exit /b %ERRORLEVEL%')
$live=@('@echo off','call "%~dp0Cold-Start.cmd" -AuthorizedLiveTest %*','exit /b %ERRORLEVEL%')
Set-Content -LiteralPath (Join-Path $bundle 'Cold-Start.cmd') -Value $cold -Encoding ascii
Set-Content -LiteralPath (Join-Path $bundle 'Live-Test.cmd') -Value $live -Encoding ascii
Copy-Item -LiteralPath (Join-Path $root 'Stop-AudioCore.cmd') -Destination $bundle
Copy-Item -LiteralPath (Join-Path $root ('docs/RELEASE-'+$Version+'.md')) -Destination (Join-Path $bundle 'README.md')
Copy-Item -LiteralPath (Join-Path $root 'docs') -Destination $bundle -Recurse
Copy-Item -LiteralPath (Join-Path $root 'validation') -Destination $bundle -Recurse
$qtNotices=Join-Path $bundle 'notices/Qt';New-Item -ItemType Directory -Path $qtNotices -Force | Out-Null
Copy-Item -LiteralPath 'C:/Qt/Licenses/LICENSE' -Destination (Join-Path $qtNotices 'Qt-license-overview.txt')
foreach($module in @('qtdoc','qtcore','qtgui','qtwidgets','qtnetwork','qtsvg')){
    $moduleDocs=Join-Path $QtDocs $module
    $noticeFiles=@(Get-ChildItem -LiteralPath $moduleDocs -File | Where-Object {$_.Name -match '(^lgpl\.html$|^gpl\.html$|licenses-used|attribution-|license)'})
    if($noticeFiles.Count){$dest=Join-Path $qtNotices $module;New-Item -ItemType Directory -Path $dest -Force | Out-Null;Copy-Item -LiteralPath $noticeFiles.FullName -Destination $dest}
}
Copy-Item -LiteralPath (Join-Path $IppRoot 'share/doc/ipp/licensing') -Destination (Join-Path $bundle 'notices/Intel-IPP') -Recurse
foreach($component in @('gcc','winpthreads','mingw-w64')){
    $licenses=Join-Path $MingwRoot ('licenses/'+$component)
    if(Test-Path -LiteralPath $licenses){Copy-Item -LiteralPath $licenses -Destination (Join-Path $bundle ('notices/MinGW-'+$component)) -Recurse}
}
$forbidden=@(Get-ChildItem -LiteralPath $bundle -Recurse -File | Where-Object {
    $relative=$_.FullName.Substring($bundle.Length+1)
    ($_.Extension -in @('.lib','.pdb','.obj','.entropy') -and $relative -notlike 'notices\*') -or $relative -match '(^|[\\/])(build|build-msvc[^\\/]*|include|IntelSDK)[\\/]'
})
if($forbidden.Count){throw 'SDK/build/private profile file detected in bundle'}
$sourceCommit=(& git -C $root rev-parse HEAD).Trim()
$dirty=[bool](& git -C $root status --porcelain)
$files=@(Get-ChildItem -LiteralPath $bundle -Recurse -File | Sort-Object FullName | ForEach-Object {[ordered]@{path=$_.FullName.Substring($bundle.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
$numeric=if($Version -eq '0.0.1'){'PASS'}else{'NOT RERUN; historical 0.0.1 PASS'}
$manifest=[ordered]@{version=$Version;channel=$Channel;sourceCommit=$sourceCommit;sourceDirty=$dirty;architecture='Windows x64; native Core, separate IPC-only Qt UI';stage04='FAIL REALTIME';numeric=$numeric;v07='BLOCKED';automaticArm=$false;files=$files}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
$zip=Join-Path $output ('EntropyAudioCore-'+$Version+'-beta-win-x64.zip')
Compress-Archive -LiteralPath $bundle -DestinationPath $zip -CompressionLevel Optimal
$sha=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Value ($sha+'  '+[IO.Path]::GetFileName($zip)) -Encoding ascii
$result=[ordered]@{bundle=$bundle;zip=$zip;sha256=$sha;checksums=Join-Path $output 'SHA256SUMS.txt';sourceCommit=$sourceCommit;sourceDirty=$dirty}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'package-result.json') -Encoding utf8
Write-Output ($result | ConvertTo-Json)
