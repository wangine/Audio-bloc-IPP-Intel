param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Build-Model.ps1') -Configuration $Configuration
