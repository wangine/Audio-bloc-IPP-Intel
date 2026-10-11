param([Parameter(Mandatory=$true)][string]$Package,
      [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{40}$')][string]$CertificateThumbprint,
      [ValidatePattern('^r[1-9][0-9]*$')][string]$Revision='r1')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path (Split-Path $root -Parent) -Parent
$Package=[IO.Path]::GetFullPath($Package)
$workspace=[IO.Path]::GetFullPath((Split-Path $repo -Parent)).TrimEnd('\')+'\'
if(!$Package.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'Package outside named workspace'}
. (Join-Path $PSScriptRoot 'Assert-Package.ps1')
$manifest=Assert-MusicPackage $Package
if($manifest.sourceWorkingTreeDirty){throw 'Commit package sources before signing'}
if($manifest.signatures.'driver/EntropyMusicRenderV2.sys' -ne 'NotSigned'){throw 'Historical signed packages are not modified'}
$certificate=Get-Item -LiteralPath ('Cert:/LocalMachine/My/'+$CertificateThumbprint)
if(!$certificate.HasPrivateKey -or $certificate.Subject -ne 'CN=Entropy Audio DEV Driver Signing'){throw 'Expected existing DEV signing certificate unavailable'}
$sdk=Join-Path $root 'build/kits/sdk'
$sign=Join-Path $sdk 'bin/10.0.28000.0/x64/signtool.exe'
$catalog=Join-Path $sdk 'bin/10.0.28000.0/x86/Inf2Cat.exe'
$driver=Join-Path $Package 'driver'
& $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My /sm (Join-Path $driver 'EntropyMusicRenderV2.sys') *> (Join-Path $Package 'provenance/sign-sys.log')
if($LASTEXITCODE){throw 'SYS signing failed'}
& $catalog "/driver:$driver" '/os:10_CO_X64,10_NI_X64,10_GE_X64' *> (Join-Path $Package 'provenance/catalog-generation-signed.log')
if($LASTEXITCODE){throw 'Signed SYS catalog generation failed'}
& $sign sign /fd SHA256 /sha1 $CertificateThumbprint /s My /sm (Join-Path $driver 'EntropyMusicRenderV2.cat') *> (Join-Path $Package 'provenance/sign-cat.log')
if($LASTEXITCODE){throw 'CAT signing failed'}
foreach($file in @('EntropyMusicRenderV2.sys','EntropyMusicRenderV2.cat')){
    & $sign verify /pa /v (Join-Path $driver $file) *> (Join-Path $Package ('provenance/verify-'+$file+'.log'))
    if($LASTEXITCODE -or (Get-AuthenticodeSignature -LiteralPath (Join-Path $driver $file)).Status -ne 'Valid'){throw "DEV signature verification failed: $file"}
}
$manifest.version=$manifest.version.Replace('-candidate',('-signed-dev-'+$Revision))
$manifest | Add-Member -Force -NotePropertyName certificateThumbprint -NotePropertyValue $CertificateThumbprint
$manifest | Add-Member -Force -NotePropertyName signingPolicy -NotePropertyValue 'Existing trusted DEV/test-signing policy; no certificate import or boot change; not Microsoft production signing'
foreach($file in @('driver/EntropyMusicRenderV2.sys','driver/EntropyMusicRenderV2.cat')){$manifest.signatures.$file='Valid'}
$manifest.artifacts=@(Get-ChildItem -LiteralPath $Package -Recurse -File | Where-Object Name -notin @('manifest.json','SHA256SUMS.txt') | Sort-Object FullName | ForEach-Object {@{path=[IO.Path]::GetRelativePath($Package,$_.FullName).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $Package 'manifest.json') -Encoding utf8
$manifest.artifacts | ForEach-Object {"$($_.sha256)  $($_.path)"} | Set-Content -LiteralPath (Join-Path $Package 'SHA256SUMS.txt') -Encoding ascii
$null=Assert-MusicPackage $Package
Write-Output "Signed DEV package verified: $Package; installation and live acceptance not performed by this tool."
