param([string]$StateDirectory = (Join-Path $PSScriptRoot 'state'), [string]$PublishEndpoint)
& "$PSScriptRoot\Stage1-Launch.ps1" -Action UI -StateDirectory $StateDirectory -PublishEndpoint $PublishEndpoint
