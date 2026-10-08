param([string]$ResultsFolder='local/stage04-results',[string]$BuildFolder='build/msvc-stage04')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    New-Item -ItemType Directory -Force local/stage04-archives | Out-Null
    $stamp=Get-Date -Format yyyyMMdd-HHmmss
    $archive="local/stage04-archives/results-$stamp.zip"
    $package="local/stage04-archives/package-$stamp"
    New-Item -ItemType Directory -Path $package | Out-Null
    foreach($item in @(
        @{Source=$ResultsFolder;Name='Measured'},
        @{Source="$BuildFolder/stage02-results";Name='Stage02Regression'},
        @{Source="$BuildFolder/stage04-results";Name='Stage04CTest'},
        @{Source="$BuildFolder/Testing";Name='CTestDetail'}
    )) {Copy-Item -LiteralPath $item.Source -Destination (Join-Path $package $item.Name) -Recurse}
    $logs=Join-Path $package 'BuildAndInitialLogs'
    New-Item -ItemType Directory -Path $logs | Out-Null
    Get-ChildItem -LiteralPath $BuildFolder -File -Filter '*.txt' | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $logs}
    # Raw evidence only: no SDK, object files, libraries or executables.
    Compress-Archive -Path "$package/*" -DestinationPath $archive -CompressionLevel Optimal
    Get-FileHash -LiteralPath $archive -Algorithm SHA256 | Select-Object Path,Hash | Export-Csv local/stage04-archive-hash.csv -NoTypeInformation
    Get-Content local/stage04-archive-hash.csv
} finally {Pop-Location}
