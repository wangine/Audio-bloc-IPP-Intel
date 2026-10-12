param([ValidateSet('Debug','Release')][string]$Configuration='Release',[ValidateSet('x64')][string]$Platform='x64',[switch]$ReferenceOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $root -Parent) -Parent
$logs=Join-Path $repo 'validation/music-render-v2'
New-Item -ItemType Directory -Force $logs | Out-Null
$vswhere='C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if(!$vs){throw 'MSVC installation unavailable'}
$msbuild=Join-Path $vs 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$wdk='10.0.28000.0'
if(!(Test-Path -LiteralPath "C:/Program Files (x86)/Windows Kits/10/Include/$wdk/km/portcls.h")){throw 'Approved WDK headers unavailable'}
$sdk=Join-Path $root 'build/kits/sdk/'
if(!(Test-Path -LiteralPath "$sdk/Include/$wdk/shared/sdkddkver.h") -or !(Test-Path -LiteralPath "$sdk/Lib/$wdk/um/x64/gdi32.lib")){throw 'Matching SDK missing; run tools/Prepare-LocalSdk.ps1 (workspace only)'}
$reference=Join-Path $root 'upstream/audio/simpleaudiosample/SimpleAudioSample.sln'
$candidate=Join-Path $root 'kernel/EntropyMusicRenderV2.vcxproj'
$project=if($ReferenceOnly){$reference}else{$candidate}
if(!(Test-Path -LiteralPath $project)){throw 'Requested kernel project not implemented/imported'}
$label=if($ReferenceOnly){'kernel-reference'}else{'kernel'}
$output=Join-Path $root "build/$label/"
New-Item -ItemType Directory -Force $output | Out-Null
$log=Join-Path $logs "$label-build.log"
# The unmodified sample links sibling LIBs by their default relative paths.
$outputArgs=@()
if(!$ReferenceOnly){$outputArgs=@("/p:OutDir=$output")}
& $msbuild $project /m:2 /nr:false /nologo /v:minimal "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/p:WindowsTargetPlatformVersion=$wdk" "/p:WindowsSdkDir=$sdk" "/p:WindowsSdkDir_10=$sdk" "/p:UniversalCRTSdkDir=$sdk" "/p:UCRTContentRoot=$sdk" "/p:WDKContentRoot=$sdk" "/p:WindowsSdkVerBinPath=$sdk/bin/$wdk/" '/p:PreferredToolArchitecture=x64' "/p:InfToolPath=$sdk/bin/$wdk/x64/" '/p:InfToolArchitecture=Native64Bit' "/p:RCToolPath=$sdk/bin/$wdk/x64/" '/p:RCToolArchitecture=Native64Bit' '/p:SignMode=Off' '/p:EnableInf2Cat=false' @outputArgs *> $log
$code=$LASTEXITCODE
if($ReferenceOnly -and !$code){Get-ChildItem -LiteralPath (Join-Path $root 'upstream') -Recurse -Filter *.sys -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $output -Force}}
$binaries=@(Get-ChildItem -LiteralPath $output -Recurse -Filter *.sys -File | ForEach-Object {@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash $_.FullName).Hash}})
@{project=$project;msbuild=$msbuild;visualStudio=$vs;wdk=$wdk;sdk=$sdk;sdkProvenance=(Join-Path $root 'build/kits/sdk-provenance.json');configuration=$Configuration;platform=$Platform;exit=$code;signing='Off; no deployment';binaries=$binaries} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $logs "$label-build.json") -Encoding utf8
Get-Content -LiteralPath $log -Tail 18
if($code){throw "Kernel build failed ($code), see $log"}
if(!$binaries.Count){throw 'Build did not produce a SYS binary'}
$binaries | ConvertTo-Json -Depth 3
