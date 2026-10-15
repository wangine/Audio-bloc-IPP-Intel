param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$SdkDirectory='D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\Drivers\EntropyMusicRenderV2\build\kits\sdk',
 [string]$OutputRoot='D:\Projects\Entropy-ClockFollow-C1-20261015\candidate'
)
$ErrorActionPreference='Stop'
$driver=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $driver -Parent) -Parent
$base='fa9e126d4bcbde2aaaf033de78509c6f601f8dbb'
$branch='chatgpt/music-clockfollow-c1-20261015'
$installedBaselineSha='FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA'
$old=Get-Location
function Run-Native([string]$Program,[string[]]$Arguments,[string]$Log){
 $preference=$ErrorActionPreference
 try {$ErrorActionPreference='Continue'; & $Program @Arguments *> $Log; $code=$LASTEXITCODE}
 finally {$ErrorActionPreference=$preference}
 Get-Content -LiteralPath $Log -Tail 15
 if($code -ne 0){throw "Command failed ($code): $Program; see $Log. Stop without source repairs."}
}
try {
 Set-Location -LiteralPath $repo
 $head=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Wrong source SHA; no build'}
 $name=(& git branch --show-current)
 if($LASTEXITCODE -ne 0 -or $name -ne $branch){throw 'Wrong ClockFollow branch'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked changes present; do not reset/stash/delete them'}
 & git merge-base --is-ancestor $base HEAD
 if($LASTEXITCODE -ne 0){throw 'Installed driver source baseline is not an ancestor'}
 $changes=@(& git diff --name-only $base HEAD)
 if($LASTEXITCODE -ne 0){throw 'Cannot inspect source scope'}
 foreach($path in $changes){if(!$path.StartsWith('Drivers/EntropyMusicRenderV2/') -and $path -ne 'AGENTS.md' -and $path -ne '.github/workflows/music-clockfollow-c1.yml'){throw "Change outside C1 scope: $path"}}
 & git diff --quiet $base HEAD -- Drivers/EntropyMusicRenderV2/provider Drivers/EntropyMusicRenderV2/upstream Drivers/EntropyMusicRenderV2/protocol/PcmBridgeV1.h Drivers/EntropyMusicRenderV2/protocol/PcmBridgeV1.cpp Drivers/EntropyMusicRenderV2/model/Format.cpp Drivers/EntropyMusicRenderV2/model/PcmRing.cpp Drivers/EntropyMusicRenderV2/model/PacketTimeline.cpp
 if($LASTEXITCODE -ne 0){throw 'Existing RAW provider, PCM ABI, imported shell or payload model changed'}
 $cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
 $ninja='C:\Qt\Tools\Ninja\ninja.exe'
 $vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
 foreach($file in @($cmake,$ninja,$vswhere)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Existing build tool missing: $file"}}
 $vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
 if($LASTEXITCODE -ne 0 -or !$vs){throw 'MSVC unavailable'}
 $vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
 if(!(Test-Path -LiteralPath $vcvars -PathType Leaf)){throw 'MSVC x64 environment unavailable'}
 $SdkDirectory=[IO.Path]::GetFullPath($SdkDirectory)
 foreach($file in @((Join-Path $SdkDirectory 'Include/10.0.28000.0/shared/sdkddkver.h'),(Join-Path $SdkDirectory 'Lib/10.0.28000.0/um/x64/gdi32.lib'),'C:\Program Files (x86)\Windows Kits\10\Include\10.0.28000.0\km\portcls.h')){
  if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Approved existing SDK/WDK dependency missing: $file. No install or substitution."}
 }
 $provenance=Join-Path (Split-Path $SdkDirectory -Parent) 'sdk-provenance.json'
 if(!(Test-Path -LiteralPath $provenance -PathType Leaf)){throw 'Existing SDK provenance record missing'}
 $sdkProvenanceHash=(Get-FileHash -LiteralPath $provenance).Hash
 $bundle=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) 'ClockFollow-C1-0.3.1.0'
 if(Test-Path -LiteralPath $bundle){throw 'C1 output exists; preserve it and choose a new explicit OutputRoot'}
 New-Item -ItemType Directory -Path $bundle|Out-Null
 $logs=Join-Path $bundle 'logs';$artifacts=Join-Path $bundle 'unsigned-driver'
 New-Item -ItemType Directory -Path $logs,$artifacts|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'C1 compilation incomplete. Do not install, sign or load.'
 $modelBuild=Join-Path $driver 'build/clockfollow-c1'
 $runner=Join-Path $logs 'build-model.cmd'
 @('@echo off',('call "'+$vcvars+'" || exit /b 1'),'set "PATH=C:\Qt\Tools\Ninja;%PATH%"',
  ('"'+$cmake+'" -S "'+(Join-Path $driver 'clockfollow-tests')+'" -B "'+$modelBuild+'" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1'),
  ('"'+$cmake+'" --build "'+$modelBuild+'" --parallel 2 --target EntropyClockFollowContracts || exit /b 1'))|Set-Content -LiteralPath $runner -Encoding ascii
 Run-Native -Program 'cmd.exe' -Arguments @('/d','/c',('"'+$runner+'"')) -Log (Join-Path $logs 'model-build.log')
 $model=Join-Path $modelBuild 'EntropyClockFollowContracts.exe'
 if(!(Test-Path -LiteralPath $model -PathType Leaf)){throw 'Clock model contract executable missing'}
 Run-Native -Program $model -Arguments @() -Log (Join-Path $logs 'model-contracts.log')
 # Compile only: SignMode=Off; EnableInf2Cat=false; no device registration.
 & (Join-Path $PSScriptRoot 'Build-Kernel.ps1') -Configuration Release -Platform x64 -SdkDirectory $SdkDirectory -LogDirectory $logs
 if(!$?){throw 'C1 kernel build failed'}
 $kernel=Join-Path $driver 'build/kernel'
 $sys=@(Get-ChildItem -LiteralPath $kernel -Recurse -File -Filter EntropyMusicRenderV2.sys)
 $inf=@(Get-ChildItem -LiteralPath $kernel -Recurse -File -Filter EntropyMusicRenderV2.inf)
 if($sys.Count -ne 1 -or $inf.Count -ne 1){throw 'Expected exactly one new SYS and stamped INF'}
 $version=$sys[0].VersionInfo
 if($version.FileMajorPart -ne 0 -or $version.FileMinorPart -ne 3 -or $version.FileBuildPart -ne 1 -or $version.FilePrivatePart -ne 0){throw 'SYS file version does not identify C1 0.3.1.0'}
 $infText=Get-Content -LiteralPath $inf[0].FullName -Raw
 if($infText -notmatch '(?im)^\s*DriverVer\s*=\s*[^,\r\n]+,\s*0\.3\.1\.0\s*$'){throw 'Stamped INF version does not identify C1 0.3.1.0'}
 $sysHash=(Get-FileHash -LiteralPath $sys[0].FullName).Hash
 if($sysHash -eq $installedBaselineSha){throw 'Candidate matches the old installed SYS'}
 $signature=Get-AuthenticodeSignature -LiteralPath $sys[0].FullName
 if($signature.Status -ne 'NotSigned'){throw "Unexpected candidate signature status: $($signature.Status). This task is unsigned build only."}
 Copy-Item -LiteralPath $sys[0].FullName,$inf[0].FullName -Destination $artifacts
 Copy-Item -LiteralPath $model -Destination $bundle
 Copy-Item -LiteralPath $provenance -Destination (Join-Path $logs 'sdk-provenance.json')
 foreach($name in @('CLOCKFOLLOW-C1.md','CLOCKFOLLOW-C1-DESIGN.md','CLOCKFOLLOW-C1-CODEX.md')){Copy-Item -LiteralPath (Join-Path $driver $name) -Destination $bundle}
 Copy-Item -LiteralPath (Join-Path $driver 'protocol/ClockBridgeV1.h') -Destination $bundle
 $manifest=[ordered]@{
  schema='entropy-clockfollow-c1-compile-v1';sourceCommit=$head;sourceBase=$base;branch=$branch;version='0.3.1.0';
  defaultMode='AUTONOMOUS QPC';requiresExplicitControlLease=$true;driverBuild='0x00030100';
  kernelBuild='PASS';modelContracts='PASS';signing='Off / NotSigned';catalogGenerated=$false;installed=$false;liveAudioTested=$false;
  sysSha256=$sysHash;infSha256=(Get-FileHash -LiteralPath $inf[0].FullName).Hash;modelSha256=(Get-FileHash -LiteralPath $model).Hash;
  installedBaselineSha256=$installedBaselineSha;sdkDirectory=$SdkDirectory;sdkProvenanceSha256=$sdkProvenanceHash;
  stagesChanged=$false;auxChanged=$false;cinemaChanged=$false;clockBridgeImplemented=$false;stage3DirectPrimaryImplemented=$false
 }
 $manifest|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 Set-Content -LiteralPath (Join-Path $bundle 'NOT-FOR-INSTALLATION.txt') -Encoding ascii -Value 'Unsigned C1 compile candidate only. No catalog/signing/deployment approval. Existing driver remains installed; no Stage3 clock feedback is connected.'
 if((& git rev-parse HEAD) -ne $head -or $LASTEXITCODE -ne 0){throw 'Source revision changed during build'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source files changed during build'}
 if((Get-FileHash -LiteralPath $provenance).Hash -ne $sdkProvenanceHash){throw 'SDK provenance changed during build'}
 if((Get-FileHash -LiteralPath (Join-Path $artifacts 'EntropyMusicRenderV2.sys')).Hash -ne $sysHash){throw 'Packaged SYS hash mismatch'}
 Get-ChildItem -LiteralPath $bundle -Recurse -File|Where-Object {$_.Name -notin @('INCOMPLETE.txt','SHA256SUMS.txt')}|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/')
 }|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "C1 compile candidate: $bundle"
 Write-Host "Source: $head"
 Write-Host "Unsigned SYS SHA256: $sysHash"
 Write-Host 'STOP. No installation, signing, device access, playback or Stage1/2/3 change was performed.'
} finally {Set-Location -LiteralPath $old.Path}
