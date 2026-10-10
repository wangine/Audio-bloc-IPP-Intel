param([string]$OutputRoot='D:/Projects/3Stage-3/Audio-bloc-IPP-Intel/release',[string]$Build32='build/msvc-egress-32768',[string]$Build64='build/msvc-egress-65536',[string]$UiBuild='build/qt-thiran')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$destination=[IO.Path]::GetFullPath((Join-Path $OutputRoot '0.1.8'))
if((Test-Path -LiteralPath $destination) -and @(Get-ChildItem -LiteralPath $destination -Force).Count){throw 'Preserve existing0.1.8; choose a fresh output root.'}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach($variant in @(@{name='32k';frames=32768;build=$Build32},@{name='64k';frames=65536;build=$Build64})){
 # Short staging path avoids Windows PowerShell's legacy260-character copy
 # limit for historical validation files. Keep every stage inside this workspace.
 $relativeStaging='../../../local/p019-'+$variant.name+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
 $staging=[IO.Path]::GetFullPath((Join-Path $root $relativeStaging))
 & (Join-Path $PSScriptRoot 'Package-Beta.ps1') -Version 0.1.8 -NumericStatus 'PASS targeted software contract; driver4096 unsupported; LIVE PENDING' -OutputRoot $relativeStaging -NativeBuild $variant.build -UiBuild $UiBuild | Out-Null
 $original=Join-Path $staging '0.1.8/EntropyAudioCore-0.1.8-beta-win-x64'
 $bundle=Join-Path $destination ('EntropyAudioCore-0.1.8-'+$variant.name+'-win-x64')
 Copy-Item -LiteralPath $original -Destination $bundle -Recurse
 $manifestPath=Join-Path $bundle 'manifest.json';$manifest=Get-Content $manifestPath -Raw | ConvertFrom-Json
 $manifest | Add-Member -NotePropertyName variant -NotePropertyValue $variant.name
 $manifest | Add-Member -NotePropertyName fifoCapacityFrames -NotePropertyValue $variant.frames
 $manifest | Add-Member -NotePropertyName targetFrames -NotePropertyValue 4096
 $manifest.files=@(Get-ChildItem $bundle -Recurse -File | Where-Object {$_.FullName -ne $manifestPath} | Sort-Object FullName | ForEach-Object {[ordered]@{path=$_.FullName.Substring($bundle.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 $manifest | ConvertTo-Json -Depth 8 | Set-Content $manifestPath -Encoding utf8
 $zip=Join-Path $destination ('EntropyAudioCore-0.1.8-'+$variant.name+'-win-x64.zip');Compress-Archive -LiteralPath $bundle -DestinationPath $zip -CompressionLevel Optimal
 $sha=(Get-FileHash -LiteralPath $zip).Hash;Add-Content (Join-Path $destination 'SHA256SUMS.txt') ($sha+'  '+[IO.Path]::GetFileName($zip)) -Encoding ascii
 Write-Output $bundle
}
