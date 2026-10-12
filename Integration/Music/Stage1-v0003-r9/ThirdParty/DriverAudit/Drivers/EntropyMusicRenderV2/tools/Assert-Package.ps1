function Assert-MusicPackage([string]$Folder) {
    $Folder=[IO.Path]::GetFullPath($Folder).TrimEnd('\')
    $base=$Folder+'\'
    $manifest=Get-Content -LiteralPath (Join-Path $Folder 'manifest.json') -Raw | ConvertFrom-Json
    if($manifest.identity.hardwareId -ne 'ROOT\EntropyMusicRenderV2' -or $manifest.identity.service -ne 'EntropyMusicRenderV2' -or $manifest.identity.channels -ne 2){throw 'Wrong package identity'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($item in $manifest.artifacts){
        if(!$item.path -or $item.path.Contains(':') -or [IO.Path]::IsPathRooted($item.path) -or !$seen.Add($item.path)){throw 'Invalid or duplicate artifact path'}
        $path=[IO.Path]::GetFullPath((Join-Path $Folder $item.path))
        if(!$path.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Artifact escapes package'}
        $file=Get-Item -LiteralPath $path -ErrorAction Stop
        if($file.PSIsContainer -or $file.Length -ne $item.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256){throw "Package artifact differs: $($item.path)"}
    }
    foreach($required in @('driver/EntropyMusicRenderV2.sys','user/EntropyMusicPcmAudit.exe','user/EntropyMusicRawProviderV2Audit.dll','tools/Run-Live-Acceptance.ps1','tools/Assert-Package.ps1')){if(!$seen.Contains($required)){throw "Unmanifested required artifact: $required"}}
    if($manifest.frozenCoreSha256 -notmatch '^[A-Fa-f0-9]{64}$'){throw 'Missing frozen Core hash'}
    return $manifest
}
function Assert-MusicInstalledDriver($Manifest) {
    $driver=Get-CimInstance Win32_SystemDriver -Filter "Name='EntropyMusicRenderV2'"
    if(!$driver -or $driver.State -ne 'Running'){throw 'Exact new driver service is not running'}
    $path=$driver.PathName.Trim('"')
    if($path.StartsWith('\SystemRoot\',[StringComparison]::OrdinalIgnoreCase)){$path=Join-Path $env:SystemRoot $path.Substring(12)}
    elseif(!$path.Contains(':')){$path=Join-Path $env:SystemRoot $path}
    $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $expected=@($Manifest.artifacts | Where-Object path -eq 'driver/EntropyMusicRenderV2.sys')
    if($expected.Count -ne 1 -or $hash -ne $expected[0].sha256){throw 'Installed SYS does not match this qualification package'}
    return @{service=$driver.Name;state=$driver.State;path=$path;sha256=$hash}
}
