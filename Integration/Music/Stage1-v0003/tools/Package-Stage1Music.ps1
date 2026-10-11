param([Parameter(Mandatory=$true)][string]$RuntimeRoot,
      [Parameter(Mandatory=$true)][string]$DriverPackage,
      [Parameter(Mandatory=$true)][string]$Repository,
      [Parameter(Mandatory=$true)][string]$Output,
      [string]$Version='stage1-native-music-v0003-r3')
$ErrorActionPreference='Stop'
. (Join-Path $DriverPackage 'tools/Assert-Package.ps1')
$stage1DriverManifest=Assert-MusicPackage $DriverPackage
$null=Assert-MusicInstalledDriver $stage1DriverManifest
& 'C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe' (Join-Path $PSScriptRoot 'package_v0003.py') --source (Split-Path $PSScriptRoot -Parent) --runtime $RuntimeRoot --driver $DriverPackage --repository $Repository --output $Output --version $Version
if($LASTEXITCODE){throw 'Stage1 v0003 package failed'}
