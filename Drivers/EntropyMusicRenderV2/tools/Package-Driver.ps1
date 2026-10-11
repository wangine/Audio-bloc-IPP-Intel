param([string]$Output)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $root -Parent) -Parent
if(!$Output){$Output=Join-Path $root 'build/package'}
$Output=[IO.Path]::GetFullPath($Output)
$workspace=[IO.Path]::GetFullPath((Split-Path $repo -Parent)).TrimEnd('\')+'\'
if(!$Output.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'Package output must stay inside the named workspace'}
if(Test-Path -LiteralPath $Output){throw 'Use a new output directory; existing package is not overwritten'}
New-Item -ItemType Directory -Path $Output | Out-Null
$driver=Join-Path $Output 'driver';$user=Join-Path $Output 'user';$evidence=Join-Path $Output 'provenance';$scripts=Join-Path $Output 'tools'
foreach($folder in @($driver,$user,$evidence,$scripts)){New-Item -ItemType Directory -Path $folder | Out-Null}
foreach($file in @('EntropyMusicRenderV2.sys','EntropyMusicRenderV2.inf')){Copy-Item -LiteralPath (Join-Path $root "build/kernel/$file") -Destination $driver}
foreach($file in @('EntropyMusicRawProviderV2.dll','EntropyMusicRawProviderV2Audit.dll','EntropyMusicPcmAudit.exe')){Copy-Item -LiteralPath (Join-Path $root "build/model/$file") -Destination $user}
Copy-Item -LiteralPath (Join-Path $root 'DEPLOYMENT.md') -Destination $Output
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $Output
Copy-Item -LiteralPath (Join-Path $root 'upstream/LICENSE') -Destination (Join-Path $Output 'Microsoft-MS-PL.txt')
Copy-Item -LiteralPath (Join-Path $root 'upstream-manifest.json') -Destination $evidence
Copy-Item -LiteralPath (Join-Path $root 'audit/acceptance-matrix.json') -Destination $evidence
Copy-Item -LiteralPath (Join-Path $repo 'validation/music-render-v2/kernel-build.json') -Destination $evidence
Copy-Item -LiteralPath (Join-Path $root 'build/kits/sdk-provenance.json') -Destination $evidence
# Package's convenient runner resolves the repository containing its frozen Core;
# use the source checkout runner for qualification, rather than an incomplete
# standalone Stage1 runtime hidden inside the driver package.
@'
param([Parameter(Mandatory=$true)][string]$Repository,[switch]$RunLive,[switch]$Stress,[string]$Output)
$ErrorActionPreference='Stop'
$runner=Join-Path $Repository 'Drivers/EntropyMusicRenderV2/tools/Run-Live-Acceptance.ps1'
if(!(Test-Path -LiteralPath $runner)){throw 'Matching architecture source checkout is required'}
& $runner -RunLive:$RunLive -Stress:$Stress -Output $Output
'@ | Set-Content -LiteralPath (Join-Path $scripts 'Run-Qualification.ps1')
$catalog=Join-Path $root 'build/kits/sdk/bin/10.0.28000.0/x86/Inf2Cat.exe'
& $catalog "/driver:$driver" '/os:10_CO_X64,10_NI_X64,10_GE_X64' *> (Join-Path $evidence 'catalog-generation.log')
$catalogExit=$LASTEXITCODE
if($catalogExit){Get-Content -LiteralPath (Join-Path $evidence 'catalog-generation.log') -Tail 25;throw "Catalog generation failed ($catalogExit)"}
$python='C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe'
& $python (Join-Path $PSScriptRoot 'package_sources.py') $repo $Output
if($LASTEXITCODE){throw 'Source archive failed'}
$signatures=@{}
foreach($file in @('driver/EntropyMusicRenderV2.sys','driver/EntropyMusicRenderV2.cat','user/EntropyMusicRawProviderV2.dll','user/EntropyMusicRawProviderV2Audit.dll','user/EntropyMusicPcmAudit.exe')){$signatures[$file]=(Get-AuthenticodeSignature -LiteralPath (Join-Path $Output $file)).Status.ToString()}
$signtool=Join-Path $root 'build/kits/sdk/bin/10.0.28000.0/x64/signtool.exe'
& $signtool verify /kp /v /c (Join-Path $driver 'EntropyMusicRenderV2.cat') (Join-Path $driver 'EntropyMusicRenderV2.sys') *> (Join-Path $evidence 'kernel-signature.log')
$verificationExit=$LASTEXITCODE
$revision=(& git -C $repo rev-parse HEAD).Trim()
$sourceDirty=!!(& git -C $repo status --porcelain -- Drivers/EntropyMusicRenderV2 Integration/Music/Stage1/ABI/endpoint_provider_v2.h)
$artifacts=@(Get-ChildItem -LiteralPath $Output -Recurse -File | Sort-Object FullName | ForEach-Object {@{path=[IO.Path]::GetRelativePath($Output,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$manifest=@{version='0.3.0.1-candidate';acceptance='NOT_RUN';identity=@{hardwareId='ROOT\EntropyMusicRenderV2';service='EntropyMusicRenderV2';channels=2;privateGuid='ab93289e-2692-47a9-bc47-ed1711f6e430'};sourceRevision=$revision;sourceWorkingTreeDirty=$sourceDirty;catalogGenerationExit=$catalogExit;kernelSignatureVerificationExit=$verificationExit;signatures=$signatures;artifacts=$artifacts}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Output 'manifest.json') -Encoding utf8
$artifacts | ForEach-Object {"$($_.sha256)  $($_.path)"} | Set-Content -LiteralPath (Join-Path $Output 'SHA256SUMS.txt') -Encoding ascii
& $python (Join-Path $root 'tests/PackageTests.py') --package $Output
if($LASTEXITCODE){throw 'Package validation failed'}
Write-Output "Package: $Output; catalog generated, signing status $($signatures['driver/EntropyMusicRenderV2.cat']); kernel verification exit=$verificationExit; NOT INSTALLED/NOT LIVE QUALIFIED."
