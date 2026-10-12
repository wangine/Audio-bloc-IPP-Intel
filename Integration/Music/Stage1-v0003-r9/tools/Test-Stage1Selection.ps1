param(
    [string]$StateDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'release\stage1-native-music-v0002\state'),
    [string]$ExpectedProvider = 'legacy.music-transport',
    [string]$ExpectedEndpoint = 'entropy-music-transport:main',
    [int]$ExpectedMode = 8
)
$ErrorActionPreference = 'Stop'
# Run after selecting the matching MUSIC entry and pressing Select in the real Qt UI.
# Regression oracle: a click lost to an in-flight STATUS leaves the persisted selection empty.
$sections = @{}
$current = ''
foreach ($line in Get-Content -LiteralPath (Join-Path $StateDirectory 'Stage1Music.ini')) {
    if ($line -match '^\[(.+)\]$') { $current = $Matches[1]; $sections[$current] = @{} }
    elseif ($line -match '^([^=]+)=(.*)$') { $sections[$current][$Matches[1]] = $Matches[2] }
}
if ($sections.music.providerId -ne $ExpectedProvider -or $sections.music.endpointId -ne $ExpectedEndpoint -or [int]$sections.music.mode -ne $ExpectedMode) {
    throw "FAIL Qt Select did not persist the requested MUSIC selection: provider='$($sections.music.providerId)', endpoint='$($sections.music.endpointId)', mode='$($sections.music.mode)'"
}
if ($sections.aux.providerId -or $sections.aux.endpointId) { throw 'Unexpected AUX selection during single-source test' }
Write-Output 'PASS real Qt MUSIC selection reached Core and persisted; AUX remains None.'
