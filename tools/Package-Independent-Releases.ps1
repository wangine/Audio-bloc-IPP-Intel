param(
 [string]$Stage1Root='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 1 Sources/releases/stage1-passive-publish-v0002-20261009',
 [string]$Stage2Root='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 2 DSP/versions/v0008-passive-source',
 [string]$OutputRoot='local/stage011/publication'
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$records=@()
foreach($item in @(
 @{root=$Stage1Root;name='EntropyStage1-v0002-passive-win-x64';manifest='Stage1/runtime/stage1/package-manifest.json'},
 @{root=$Stage2Root;name='EntropyStage2-v0008-passive-source-win-x64';manifest='package-manifest.json'})){
 $base=[IO.Path]::GetFullPath($item.root)
 $zipPath=[IO.Path]::GetFullPath((Join-Path $OutputRoot ($item.name+'.zip')))
 if(Test-Path -LiteralPath $zipPath){throw "Preserve existing archive: $zipPath"}
 $manifest=Get-Content -LiteralPath (Join-Path $base $item.manifest) -Raw | ConvertFrom-Json
 $manifestRoot=Split-Path (Join-Path $base $item.manifest)
 foreach($entry in $manifest.files){
  if((Get-FileHash -LiteralPath (Join-Path $manifestRoot $entry.path)).Hash.ToLowerInvariant() -ne $entry.sha256){throw "Manifest mismatch: $($entry.path)"}
 }
 $files=Get-ChildItem -LiteralPath $base -File -Recurse | Where-Object {
  $relative=[IO.Path]::GetRelativePath($base,$_.FullName).Replace('\','/')
  $relative -notmatch '(^|/)(state|build|CMakeFiles|Testing|logs)(/|$)' -and
  $relative -notmatch '\.(entropy|Edsp|obj|pdb|lib|a|log)$' -and
  $relative -notmatch '(^|/)(diagnostics|status)\.json$|stop\.request$'
 }
 $destination=[IO.File]::Open($zipPath,[IO.FileMode]::CreateNew)
 $archive=[IO.Compression.ZipArchive]::new($destination,[IO.Compression.ZipArchiveMode]::Create,$false)
 try {
  foreach($file in $files){
   $relative=[IO.Path]::GetRelativePath($base,$file.FullName).Replace('\','/')
   $entry=$archive.CreateEntry($item.name+'/'+$relative,[IO.Compression.CompressionLevel]::Optimal)
   $input=[IO.File]::Open($file.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
   $output=$entry.Open()
   try{$input.CopyTo($output)}finally{$input.Dispose();$output.Dispose()}
  }
 }finally{$archive.Dispose();$destination.Dispose()}
 $records+=@{asset=[IO.Path]::GetFileName($zipPath);sha256=(Get-FileHash -LiteralPath $zipPath).Hash;files=$files.Count;sourceCommit=$manifest.source_commit}
 Write-Output "Prepared $zipPath ($($files.Count) files)"
}
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputRoot 'assets.json') -Encoding utf8
$records | ForEach-Object {"$($_.sha256.ToLowerInvariant())  $($_.asset)"} | Set-Content -LiteralPath (Join-Path $OutputRoot 'SHA256SUMS.txt') -Encoding ascii
