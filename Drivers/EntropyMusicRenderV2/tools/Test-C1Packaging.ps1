$ErrorActionPreference='Stop'
# Exercise the actual artifact-selection block, not a copied implementation.
# Only temporary text fixtures are created; no compiler or device is invoked.
$scriptPath=Join-Path $PSScriptRoot 'Build-C1.ps1'
$text=Get-Content -LiteralPath $scriptPath -Raw
$begin=$text.IndexOf('$kernel=Join-Path $driver ''build/kernel''',[StringComparison]::Ordinal)
$end=$text.IndexOf('$version=$sys[0].VersionInfo',[StringComparison]::Ordinal)
if($begin -lt 0 -or $end -le $begin){throw 'Cannot locate the production artifact selection block'}
$code='param([string]$driver)'+[Environment]::NewLine+$text.Substring($begin,$end-$begin)+[Environment]::NewLine+'[pscustomobject]@{infCount=$inf.Count;sysCount=$sys.Count;selected=$inf[0].FullName}'
$select=[ScriptBlock]::Create($code)
$root=Join-Path ([IO.Path]::GetTempPath()) ('entropy-c1-package-'+[guid]::NewGuid().ToString('N'))
$kernel=Join-Path $root 'build/kernel'
$nested=Join-Path $kernel 'package'
New-Item -ItemType Directory -Path $nested|Out-Null
$inf=Join-Path $kernel 'EntropyMusicRenderV2.inf'
$copy=Join-Path $nested 'EntropyMusicRenderV2.inf'
$sys=Join-Path $kernel 'EntropyMusicRenderV2.sys'
function Must-Reject([string]$Reason){
 $caught=$false
 try {$null=& $select $root} catch {$caught=$true}
 if(!$caught){throw "Did not reject $Reason"}
}
try {
 Set-Content -LiteralPath $sys -Encoding ascii -Value 'not a SYS; selection fixture only'
 Set-Content -LiteralPath $inf -Encoding ascii -Value @('[Version]','DriverVer=10/15/2026,0.3.1.0')
 $one=& $select $root
 if($one.infCount -ne 1 -or $one.sysCount -ne 1){throw 'Single artifact selection failed'}
 Copy-Item -LiteralPath $inf -Destination $copy
 $two=& $select $root
 if($two.infCount -ne 1 -or $two.selected -ne $inf){throw 'Identical INF copies must select the root deterministically'}
 if((Get-FileHash -LiteralPath $copy).Hash -ne (Get-FileHash -LiteralPath $inf).Hash){throw 'Selector altered duplicate bytes'}
 Add-Content -LiteralPath $copy -Encoding ascii -Value '; different bytes'
 Must-Reject 'different INF copies'
 Remove-Item -LiteralPath $copy
 Copy-Item -LiteralPath $sys -Destination (Join-Path $nested 'EntropyMusicRenderV2.sys')
 Must-Reject 'multiple SYS files'
 Remove-Item -LiteralPath (Join-Path $nested 'EntropyMusicRenderV2.sys')
 Remove-Item -LiteralPath $inf
 Must-Reject 'missing INF'
 Write-Host 'C1 packaging selection PASS: single INF, identical copies, differing copies, duplicate SYS, missing INF. No build/device access.'
} finally {
 if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
