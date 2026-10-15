param(
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$ExpectedCommit,
 [string]$SdkDirectory='D:\Projects\3Stage-3\Audio-bloc-IPP-Intel\Drivers\EntropyMusicRenderV2\build\kits\sdk',
 [string]$OutputRoot='D:\Projects\Entropy-ClockFollow-C1-20261015\candidate',
 [switch]$PackageOnly,
 [string]$PreviousCandidate='D:\Projects\Entropy-ClockFollow-C1-20261015\candidate\ClockFollow-C1-0.3.1.0'
)
$ErrorActionPreference='Stop'
$driver=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $driver -Parent) -Parent
$base='fa9e126d4bcbde2aaaf033de78509c6f601f8dbb'
$branch='chatgpt/music-clockfollow-c1-20261015'
$installedBaselineSha='FB78EC3CC6066CDF3A039CCE95CD13C26199F2EC630F188351155F84EBB610AA'
# Package-only is deliberately limited to the reported, already successful C1
# build. These are expected bytes, not permission to substitute another build.
$reportedSource='3faccc678242f2815cc61eeb8f49a2f05c0c85b5'
$reportedSys='F7703F8A004535031EF9F96795E0AEDF1779F75EC1050EF8E48F808ACA6A6764'
$reportedInf='5834F5F6BB05741D341000F74B69EF919FDDD4CABF32D8F552B4F25D4ABC5370'
$old=Get-Location
function Run-Native([string]$Program,[string[]]$Arguments,[string]$Log){
 if($PackageOnly){throw 'PackageOnly forbids compiler and test execution'}
 $preference=$ErrorActionPreference
 try {$ErrorActionPreference='Continue'; & $Program @Arguments *> $Log; $code=$LASTEXITCODE}
 finally {$ErrorActionPreference=$preference}
 Get-Content -LiteralPath $Log -Tail 15
 if($code -ne 0){throw "Command failed ($code): $Program; see $Log. Stop without source repairs."}
}
try {
 Set-Location -LiteralPath $repo
 $head=(& git rev-parse HEAD)
 if($LASTEXITCODE -ne 0 -or $head -ne $ExpectedCommit){throw 'Wrong source SHA; no action'}
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
 $binarySource=$head
 $priorLogs='';$priorKernel=$null
 if($PackageOnly){
  $binarySource=$reportedSource
  & git merge-base --is-ancestor $reportedSource HEAD
  if($LASTEXITCODE -ne 0){throw 'Reported C1 build commit is not an ancestor'}
  # Any compiled source/project change forbids reuse, even if versions match.
  $packageChanges=@(& git diff --name-only $reportedSource HEAD)
  if($LASTEXITCODE -ne 0){throw 'Cannot verify package-only change scope'}
  foreach($path in $packageChanges){
   if($path -notlike 'Drivers/EntropyMusicRenderV2/tools/*.ps1' -and
      $path -notlike 'Drivers/EntropyMusicRenderV2/*.md' -and
      $path -ne 'AGENTS.md' -and $path -ne '.github/workflows/music-clockfollow-c1.yml'){
    throw "PackageOnly refused: compiled source/project changed: $path"
   }
  }
  $PreviousCandidate=(Resolve-Path -LiteralPath $PreviousCandidate).Path
  if(!(Test-Path -LiteralPath (Join-Path $PreviousCandidate 'INCOMPLETE.txt') -PathType Leaf)){throw 'The preserved incomplete C1 candidate is required'}
  $priorLogs=Join-Path $PreviousCandidate 'logs'
  foreach($file in @('kernel-build.json','kernel-build.log','model-build.log','model-contracts.log','build-model.cmd')){
   if(!(Test-Path -LiteralPath (Join-Path $priorLogs $file) -PathType Leaf)){throw "Previous build evidence missing: $file"}
  }
  $priorKernel=Get-Content -LiteralPath (Join-Path $priorLogs 'kernel-build.json') -Raw|ConvertFrom-Json
  if($null -eq $priorKernel.exit -or $priorKernel.exit -ne 0 -or $priorKernel.configuration -ne 'Release' -or
     $priorKernel.platform -ne 'x64' -or $priorKernel.signing -ne 'Off; no deployment'){
   throw 'Previous kernel build does not record unsigned Release/x64 success'
  }
  if(@($priorKernel.binaries).Count -ne 1 -or $priorKernel.binaries[0].sha256 -ne $reportedSys){throw 'Previous kernel build identity differs from the reported SYS'}
  $pass='ClockFollow C1 model contracts PASS; no driver, pipe or audio device opened.'
  if(!(Select-String -LiteralPath (Join-Path $priorLogs 'model-contracts.log') -SimpleMatch $pass -Quiet)){throw 'Previous model PASS record missing; no rerun is performed'}
  $SdkDirectory=[IO.Path]::GetFullPath([string]$priorKernel.sdk)
  $provenance=[string]$priorKernel.sdkProvenance
  if(!$PSBoundParameters.ContainsKey('OutputRoot')){$OutputRoot=Join-Path $OutputRoot 'package-retry'}
 } else {
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
 }
 if(!$provenance -or !(Test-Path -LiteralPath $provenance -PathType Leaf)){throw 'Existing SDK provenance record missing'}
 $sdkProvenanceHash=(Get-FileHash -LiteralPath $provenance -Algorithm SHA256).Hash
 $bundle=Join-Path ([IO.Path]::GetFullPath($OutputRoot)) 'ClockFollow-C1-0.3.1.0'
 if(Test-Path -LiteralPath $bundle){throw 'C1 output exists; preserve it and choose a new explicit OutputRoot'}
 New-Item -ItemType Directory -Path $bundle|Out-Null
 $logs=Join-Path $bundle 'logs';$artifacts=Join-Path $bundle 'unsigned-driver'
 New-Item -ItemType Directory -Path $logs,$artifacts|Out-Null
 Set-Content -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt') -Encoding ascii -Value 'C1 package incomplete. Do not install, sign or load.'
 $modelBuild=Join-Path $driver 'build/clockfollow-c1'
 $model=Join-Path $modelBuild 'EntropyClockFollowContracts.exe'
 $evidence=@()
 if($PackageOnly){
  foreach($file in @('kernel-build.json','kernel-build.log','model-build.log','model-contracts.log','build-model.cmd')){
   $source=Join-Path $priorLogs $file;$hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
   Copy-Item -LiteralPath $source -Destination $logs
   if((Get-FileHash -LiteralPath (Join-Path $logs $file) -Algorithm SHA256).Hash -ne $hash){throw 'Evidence copy hash mismatch'}
   $evidence+=[pscustomobject]@{path=$source;sha256=$hash}
  }
  Write-Host 'PackageOnly: reusing reported build outputs. No compiler, model executable or test suite is run.'
 } else {
  $runner=Join-Path $logs 'build-model.cmd'
  @('@echo off',('call "'+$vcvars+'" || exit /b 1'),'set "PATH=C:\Qt\Tools\Ninja;%PATH%"',
   ('"'+$cmake+'" -S "'+(Join-Path $driver 'clockfollow-tests')+'" -B "'+$modelBuild+'" -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1'),
   ('"'+$cmake+'" --build "'+$modelBuild+'" --parallel 2 --target EntropyClockFollowContracts || exit /b 1'))|Set-Content -LiteralPath $runner -Encoding ascii
  Run-Native -Program 'cmd.exe' -Arguments @('/d','/c',('"'+$runner+'"')) -Log (Join-Path $logs 'model-build.log')
  if(!(Test-Path -LiteralPath $model -PathType Leaf)){throw 'Clock model contract executable missing'}
  Run-Native -Program $model -Arguments @() -Log (Join-Path $logs 'model-contracts.log')
  & (Join-Path $PSScriptRoot 'Build-Kernel.ps1') -Configuration Release -Platform x64 -SdkDirectory $SdkDirectory -LogDirectory $logs
  if(!$?){throw 'C1 kernel build failed'}
 }
 if(!(Test-Path -LiteralPath $model -PathType Leaf)){throw 'Existing clock model executable missing; no automatic rebuild'}
 $modelHash=(Get-FileHash -LiteralPath $model -Algorithm SHA256).Hash
 $kernel=Join-Path $driver 'build/kernel'
 $sys=@(Get-ChildItem -LiteralPath $kernel -Recurse -File -Filter EntropyMusicRenderV2.sys)
 $infCopies=@(Get-ChildItem -LiteralPath $kernel -Recurse -File -Filter EntropyMusicRenderV2.inf|Sort-Object FullName)
 if($sys.Count -ne 1 -or $infCopies.Count -lt 1){throw 'Expected one new SYS and at least one stamped INF'}
 # A stamped INF can also be copied into a generated package subdirectory.
 # Accept duplicates only after proving identical contents; never choose an
 # arbitrary first INF while ignoring another version or stale different copy.
 $infEvidence=@($infCopies|ForEach-Object {[pscustomobject]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
 $infHashes=@($infEvidence.sha256|Select-Object -Unique)
 $infSizes=@($infEvidence.bytes|Select-Object -Unique)
 if($infHashes.Count -ne 1 -or $infSizes.Count -ne 1){throw 'Stamped INF copies differ; preserve them and stop'}
 $canonicalInf=Join-Path $kernel 'EntropyMusicRenderV2.inf'
 $inf=@($infCopies|Where-Object {$_.FullName -eq $canonicalInf})
 if($inf.Count -eq 0){$inf=@($infCopies[0])}
 $version=$sys[0].VersionInfo
 if($version.FileMajorPart -ne 0 -or $version.FileMinorPart -ne 3 -or $version.FileBuildPart -ne 1 -or $version.FilePrivatePart -ne 0){throw 'SYS file version does not identify C1 0.3.1.0'}
 $infText=Get-Content -LiteralPath $inf[0].FullName -Raw
 if($infText -notmatch '(?im)^\s*DriverVer\s*=\s*[^,\r\n]+,\s*0\.3\.1\.0\s*$'){throw 'Stamped INF version does not identify C1 0.3.1.0'}
 $sysHash=(Get-FileHash -LiteralPath $sys[0].FullName -Algorithm SHA256).Hash
 $infHash=(Get-FileHash -LiteralPath $inf[0].FullName -Algorithm SHA256).Hash
 if($sysHash -eq $installedBaselineSha){throw 'Candidate matches the old installed SYS'}
 if($PackageOnly){
  if($sysHash -ne $reportedSys -or $infHash -ne $reportedInf){throw 'PackageOnly bytes differ from the already reported C1 build; no substitution'}
  if([IO.Path]::GetFullPath([string]$priorKernel.binaries[0].path) -ne $sys[0].FullName){throw 'SYS path differs from the previous build evidence'}
 }
 $signature=Get-AuthenticodeSignature -LiteralPath $sys[0].FullName
 if($signature.Status -ne 'NotSigned'){throw "Unexpected candidate signature status: $($signature.Status). This task is unsigned packaging only."}
 Copy-Item -LiteralPath $sys[0].FullName,$inf[0].FullName -Destination $artifacts
 Copy-Item -LiteralPath $model -Destination $bundle
 Copy-Item -LiteralPath $provenance -Destination (Join-Path $logs 'sdk-provenance.json')
 foreach($doc in @('CLOCKFOLLOW-C1.md','CLOCKFOLLOW-C1-DESIGN.md','CLOCKFOLLOW-C1-CODEX.md')){Copy-Item -LiteralPath (Join-Path $driver $doc) -Destination $bundle}
 if(Test-Path -LiteralPath (Join-Path $driver 'C1-PACKAGE-RESUME.md')){Copy-Item -LiteralPath (Join-Path $driver 'C1-PACKAGE-RESUME.md') -Destination $bundle}
 Copy-Item -LiteralPath (Join-Path $driver 'protocol/ClockBridgeV1.h') -Destination $bundle
 $result=if($PackageOnly){'PASS_RECORDED_PREVIOUS_RUN'}else{'PASS'}
 $manifest=[ordered]@{
  schema='entropy-clockfollow-c1-compile-v1';sourceCommit=$binarySource;packagingCommit=$head;sourceBase=$base;branch=$branch;version='0.3.1.0';
  packageOnly=[bool]$PackageOnly;buildExecutedThisRun=(!$PackageOnly);modelExecutedThisRun=(!$PackageOnly);
  previousCandidate=$(if($PackageOnly){$PreviousCandidate}else{$null});previousEvidence=$evidence;
  defaultMode='AUTONOMOUS QPC';requiresExplicitControlLease=$true;driverBuild='0x00030100';
  kernelBuild=$result;modelContracts=$result;signing='Off / NotSigned';catalogGenerated=$false;installed=$false;liveAudioTested=$false;
  sysSha256=$sysHash;infSha256=$infHash;modelSha256=$modelHash;selectedInf=$inf[0].FullName;allInfCopies=$infEvidence;
  installedBaselineSha256=$installedBaselineSha;sdkDirectory=$SdkDirectory;sdkProvenanceSha256=$sdkProvenanceHash;
  stagesChanged=$false;auxChanged=$false;cinemaChanged=$false;clockBridgeImplemented=$false;stage3DirectPrimaryImplemented=$false
 }
 $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Encoding utf8
 Set-Content -LiteralPath (Join-Path $bundle 'NOT-FOR-INSTALLATION.txt') -Encoding ascii -Value 'Unsigned C1 candidate only. No catalog/signing/deployment approval. Existing driver remains installed; no Stage3 clock feedback is connected.'
 if((& git rev-parse HEAD) -ne $head -or $LASTEXITCODE -ne 0){throw 'Source revision changed during packaging'}
 $dirty=@(& git status --porcelain --untracked-files=no)
 if($LASTEXITCODE -ne 0 -or $dirty.Count){throw 'Tracked source files changed during packaging'}
 if((Get-FileHash -LiteralPath $provenance -Algorithm SHA256).Hash -ne $sdkProvenanceHash){throw 'SDK provenance changed during packaging'}
 foreach($pair in @(@('unsigned-driver/EntropyMusicRenderV2.sys',$sysHash),@('unsigned-driver/EntropyMusicRenderV2.inf',$infHash),@('EntropyClockFollowContracts.exe',$modelHash),@('logs/sdk-provenance.json',$sdkProvenanceHash))){
  if((Get-FileHash -LiteralPath (Join-Path $bundle $pair[0]) -Algorithm SHA256).Hash -ne $pair[1]){throw "Packaged hash mismatch: $($pair[0])"}
 }
 foreach($item in @($evidence)+@($infEvidence)){
  if((Get-FileHash -LiteralPath $item.path -Algorithm SHA256).Hash -ne $item.sha256){throw "Original evidence/artifact changed: $($item.path)"}
 }
 Get-ChildItem -LiteralPath $bundle -Recurse -File|Where-Object {$_.Name -notin @('INCOMPLETE.txt','SHA256SUMS.txt')}|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash+'  '+$_.FullName.Substring($bundle.Length+1).Replace('\','/')
 }|Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii
 Remove-Item -LiteralPath (Join-Path $bundle 'INCOMPLETE.txt')
 Write-Host "C1 candidate packaged: $bundle"
 Write-Host "Binary source: $binarySource; packaging script: $head"
 Write-Host "Unsigned SYS SHA256: $sysHash"
 Write-Host 'STOP. No signing, installation, device access, playback or Stage1/2/3 change was performed.'
} finally {Set-Location -LiteralPath $old.Path}
