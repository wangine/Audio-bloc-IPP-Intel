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
foreach($id in @('microsoft.windows.sdk.cpp','microsoft.windows.sdk.cpp.x64','microsoft.windows.wdk.x64')) {
    $url="https://api.nuget.org/v3-flatcontainer/$id/$Version/$id.$Version.nupkg"
    $file=Join-Path $cache "$id.$Version.nupkg"
    if(!(Test-Path -LiteralPath $file)){Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $file}
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
