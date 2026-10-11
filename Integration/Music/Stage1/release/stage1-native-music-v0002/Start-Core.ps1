param([string]$StateDirectory = (Join-Path $PSScriptRoot 'state'), [string]$PublishEndpoint)
& "$PSScriptRoot\Stage1-Launch.ps1" -Action Start -StateDirectory $StateDirectory -PublishEndpoint $PublishEndpoint -NoUI
