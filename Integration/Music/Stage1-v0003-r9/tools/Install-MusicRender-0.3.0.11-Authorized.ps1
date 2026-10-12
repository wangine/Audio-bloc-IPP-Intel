# Session-specific deployment: user approved signing/updating only the new V2 device.
$ErrorActionPreference='Stop'
$repo='D:/Projects/3Stage-3/Audio-bloc-IPP-Intel'
$package=Join-Path $repo 'release/EntropyMusicRenderV2/0.3.0.11-signed-dev-r2'
$evidence=Join-Path $repo 'validation/music-render-v2'
$result=Join-Path $evidence 'install-0.3.0.11-result.json'
try {
    $identity=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if(!$identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator token required'}
    $devices=@(Get-CimInstance Win32_PnPEntity | Where-Object {$_.HardwareID -contains 'ROOT\EntropyMusicRenderV2'})
    if($devices.Count -ne 1 -or $devices[0].DeviceID -ne 'ROOT\MEDIA\0003'){throw 'Exact existing V2 device identity differs'}
    & (Join-Path $repo 'Drivers/EntropyMusicRenderV2/tools/Sign-ExistingDevPackage.ps1') -Package $package -CertificateThumbprint '89EE6700145A65ED29B0B129052933D17560AFB3' -Revision r2 *> (Join-Path $evidence 'sign-0.3.0.11.log')
    . (Join-Path $package 'tools/Assert-Package.ps1')
    $manifest=Assert-MusicPackage $package
    $devcon=Join-Path $repo 'Drivers/EntropyMusicRenderV2/build/kits/sdk/tools/10.0.28000.0/x64/devcon.exe'
    & $devcon update (Join-Path $package 'driver/EntropyMusicRenderV2.inf') 'ROOT\EntropyMusicRenderV2' *> (Join-Path $evidence 'devcon-update-0.3.0.11.log')
    $code=$LASTEXITCODE
    if($code -ne 0){throw "Device update exit=$code; no reboot performed"}
    $installed=Assert-MusicInstalledDriver $manifest
    $pnp=Get-CimInstance Win32_PnPSignedDriver | Where-Object DeviceID -eq 'ROOT\MEDIA\0003'
    if($pnp.DriverVersion -ne '0.3.0.11'){throw 'Installed PnP driver version differs'}
    @{pass=$true;deviceId=$pnp.DeviceID;driverVersion=$pnp.DriverVersion;inf=$pnp.InfName;installed=$installed;packageManifestSha256=(Get-FileHash -LiteralPath (Join-Path $package 'manifest.json')).Hash} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $result -Encoding utf8
} catch {
    @{pass=$false;error=$_.Exception.Message} | ConvertTo-Json | Set-Content -LiteralPath $result -Encoding utf8
    exit 1
}
