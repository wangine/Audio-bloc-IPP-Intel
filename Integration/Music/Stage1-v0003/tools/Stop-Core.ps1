param([string]$StateDirectory = (Join-Path $PSScriptRoot 'state'))
& "$PSScriptRoot\Stage1-Launch.ps1" -Action Stop -StateDirectory $StateDirectory -NoUI
