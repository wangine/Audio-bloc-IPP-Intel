param([Parameter(Mandatory=$true)][string]$SignedPackage,[Parameter(Mandatory=$true)][string]$Output,[Parameter(Mandatory=$true)][string]$Version)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $root -Parent) -Parent
. (Join-Path $PSScriptRoot 'Assert-Package.ps1')
$original=Assert-MusicPackage $SignedPackage
$null=Assert-MusicInstalledDriver $original
$Output=[IO.Path]::GetFullPath($Output)
$workspace=[IO.Path]::GetFullPath((Split-Path $repo -Parent)).TrimEnd('\')+'\'
if(!$Output.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must stay inside the workspace'}
if(Test-Path -LiteralPath $Output){throw 'Historical packages are not overwritten'}
if(& git -C $repo status --porcelain -- Drivers/EntropyMusicRenderV2 Integration/Music/Stage1/ABI/endpoint_provider_v2.h){throw 'Commit the Provider source before packaging'}
foreach($name in @('EntropyMusicRenderV2.sys','EntropyMusicRenderV2.cat')){
    if((Get-AuthenticodeSignature -LiteralPath (Join-Path $SignedPackage "driver/$name")).Status -ne 'Valid'){throw "Invalid signed driver: $name"}
}
Copy-Item -LiteralPath $SignedPackage -Destination $Output -Recurse
foreach($name in @('EntropyMusicRawProviderV2.dll','EntropyMusicRawProviderV2Audit.dll','EntropyMusicPcmAudit.exe')){
    Copy-Item -LiteralPath (Join-Path $root "build/model/$name") -Destination (Join-Path $Output "user/$name")
}
foreach($name in @('Run-Live-Acceptance.ps1','Assert-Package.ps1')){
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $Output "tools/$name")
}
# Recreate only archive files in the new, explicitly checked package directory.
foreach($name in @('source.zip','source-files.json')){Remove-Item -LiteralPath (Join-Path $Output $name)}
& 'C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe' (Join-Path $PSScriptRoot 'package_sources.py') $repo $Output
if($LASTEXITCODE){throw 'Source archive failed'}
$manifest=$original
$manifest.version=$Version
$manifest.acceptance='NOT_RUN'
$manifest.sourceRevision=(& git -C $repo rev-parse HEAD).Trim()
$manifest.sourceWorkingTreeDirty=$false
$kernelRevision=if($original.PSObject.Properties['kernelSourceRevision']){$original.kernelSourceRevision}else{$original.sourceRevision}
$manifest | Add-Member -Force -NotePropertyName kernelSourceRevision -NotePropertyValue $kernelRevision
$manifest | Add-Member -Force -NotePropertyName parentManifestSha256 -NotePropertyValue (Get-FileHash -LiteralPath (Join-Path $SignedPackage 'manifest.json')).Hash
$manifest.artifacts=@(Get-ChildItem -LiteralPath $Output -Recurse -File | Where-Object Name -notin @('manifest.json','SHA256SUMS.txt') | Sort-Object FullName | ForEach-Object {@{path=[IO.Path]::GetRelativePath($Output,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Output 'manifest.json') -Encoding utf8
$manifest.artifacts | ForEach-Object {"$($_.sha256)  $($_.path)"} | Set-Content -LiteralPath (Join-Path $Output 'SHA256SUMS.txt') -Encoding ascii
$fresh=Assert-MusicPackage $Output
$null=Assert-MusicInstalledDriver $fresh
Write-Output "New Provider bundle: $Output; installed signed kernel is unchanged; live acceptance NOT_RUN."
