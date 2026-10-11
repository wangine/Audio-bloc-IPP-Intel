param([string]$Commit='2dc3fd3a0cc84a2933f2194e7ec0871584979071',[string]$Destination)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if($Commit -ne '2dc3fd3a0cc84a2933f2194e7ec0871584979071'){throw 'Reference differs from approved pin'}
if(!$Destination){$Destination=Join-Path $root 'upstream'}
$scratch=Join-Path $root 'build/import'
New-Item -ItemType Directory -Force $scratch | Out-Null
$inventory=Join-Path $scratch 'tree.json'
& gh api ('repos/microsoft/Windows-driver-samples/git/trees/'+$Commit+'?recursive=1') > $inventory
if($LASTEXITCODE){throw 'Pinned Git tree request failed'}
& 'C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe' (Join-Path $PSScriptRoot 'import_reference.py') $Commit $inventory $Destination
if($LASTEXITCODE){throw 'Pinned reference import failed'}
