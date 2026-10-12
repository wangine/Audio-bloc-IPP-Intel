# Workspace-only dependency acquisition; never runs an installer.
param([string]$Version='10.0.28000.2526')
$ErrorActionPreference='Stop'
if($Version -ne '10.0.28000.2526'){throw 'Only the pinned SDK package version is supported'}
$root=Split-Path $PSScriptRoot -Parent
$cache=Join-Path $root 'build/kits'
$sdk=Join-Path $cache 'sdk'
New-Item -ItemType Directory -Force $cache,$sdk | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$manifest=@()
$expected=@{
    'microsoft.windows.sdk.cpp'='BE1B419491607EAE6F7C57844EBAB39FACE9643C51E2AF1D9176A3BA0D0B23FC'
    'microsoft.windows.sdk.cpp.x64'='A9CAE2A8C5DA7F5DC5838AE6A76D06D0D2E2FDC3D8CFC69CA6C184E4B9193A00'
    'microsoft.windows.wdk.x64'='63C939FB5A79295BF40E941DB592681272219B04EDFF095FE2F3D123E5579A90'
}
foreach($id in @('microsoft.windows.sdk.cpp','microsoft.windows.sdk.cpp.x64','microsoft.windows.wdk.x64')) {
    $url="https://api.nuget.org/v3-flatcontainer/$id/$Version/$id.$Version.nupkg"
    $file=Join-Path $cache "$id.$Version.nupkg"
    if(!(Test-Path -LiteralPath $file)){Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $file}
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $expected[$id]){throw "Pinned package hash mismatch: $id"}
    $archive=[IO.Compression.ZipFile]::OpenRead($file)
    try {
        foreach($entry in $archive.Entries) {
            if(!$entry.FullName.StartsWith('c/') -or !$entry.Name){continue}
            $relative=$entry.FullName.Substring(2)
            if($id -eq 'microsoft.windows.sdk.cpp.x64'){$relative="Lib/10.0.28000.0/$relative"}
            $target=[IO.Path]::GetFullPath((Join-Path $sdk $relative))
            if(!$target.StartsWith([IO.Path]::GetFullPath($sdk)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe archive path'}
            New-Item -ItemType Directory -Force (Split-Path $target -Parent) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
        }
    } finally {$archive.Dispose()}
    $manifest+=@{id=$id;version=$Version;url=$url;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
}
@{sdk=$sdk;packages=$manifest;license='https://aka.ms/WinSDKLicenseURL';installation='workspace extraction only'} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $cache 'sdk-provenance.json') -Encoding utf8
if(!(Test-Path -LiteralPath "$sdk/Include/10.0.28000.0/shared/sdkddkver.h") -or !(Test-Path -LiteralPath "$sdk/Lib/10.0.28000.0/um/x64/gdi32.lib")){throw 'Incomplete local SDK'}
Write-Output $sdk
