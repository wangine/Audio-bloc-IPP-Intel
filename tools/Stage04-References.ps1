param([string]$ReferenceCache = 'local/reference-repo')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    $native = '6cbac37233c8094e39828de8309a9fe13d724ad4'
    if (!(Test-Path "$ReferenceCache/HEAD") -and !(Test-Path "$ReferenceCache/.git/HEAD")) {
        & git clone --bare https://github.com/wangine/RenderEngine-Stage-3.git $ReferenceCache
        if ($LASTEXITCODE) { throw 'Cannot obtain pinned original references' }
    }
    New-Item -ItemType Directory -Force local/stage04-ref, local/stage04-baseline | Out-Null
    & git -C $ReferenceCache archive --format=tar --output="$root/local/stage04-native.tar" $native Stage3/native-reference/dependencies/SRC Stage3/native-reference/Modules/ClockSync Stage3/native-reference/Modules/DacSink Stage3/native-reference/Modules/DeviceManager Stage3/native-reference/Modules/Pcm24Converter
    if ($LASTEXITCODE) { throw 'Pinned reference archive failed' }
    & tar -xf local/stage04-native.tar -C local/stage04-ref
    if ($LASTEXITCODE) { throw 'Reference extraction failed' }
    # Pristine Stage02 oracle dependencies: extract from pinned commits, never
    # compare the authorized Stage03 storage changes to the older Crossover.
    foreach($reference in @(
        @{Folder='ref-crossover';Revision='99b84d13a6808486690e24feba35128d7675be95';Trees=@('Stage3/native-reference/Modules/CrossoverIPP')},
        @{Folder='ref-native';Revision=$native;Trees=@('Stage3/native-reference/Modules','Stage3/native-reference/Shared')}
    )) {
        $folder="local/$($reference.Folder)"
        New-Item -ItemType Directory -Force $folder | Out-Null
        & git -C $ReferenceCache archive --format=tar --output="$root/local/$($reference.Folder)-stage04.tar" $reference.Revision @($reference.Trees)
        if($LASTEXITCODE){throw 'Original numerical oracle archive failed'}
        & tar -xf "local/$($reference.Folder)-stage04.tar" -C $folder
        if($LASTEXITCODE){throw 'Original numerical oracle extraction failed'}
    }
    & git archive --format=tar --output=local/stage04-baseline.tar 429589ad0831f034384b345eb6f3b875a8b49888 DSP Egress Devices Dependencies Shared
    if ($LASTEXITCODE) { throw 'Stage03 baseline archive failed' }
    & tar -xf local/stage04-baseline.tar -C local/stage04-baseline
    if ($LASTEXITCODE) { throw 'Baseline extraction failed' }
    $rows = @(Get-ChildItem local/stage04-baseline -File -Recurse | ForEach-Object {
        $relative = $_.FullName.Substring((Join-Path $root 'local/stage04-baseline').Length+1)
        $before = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        $after = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash
        $historyOnly=$false
        if($relative.Replace('\','/') -eq 'Egress/ClockSync/include/EgressClockEstimator.h') {
            $original=[IO.File]::ReadAllText($_.FullName).Replace("`r`n","`n")
            $current=[IO.File]::ReadAllText((Join-Path $root $relative)).Replace("`r`n","`n")
            $historyOnly=$current -ceq $original.Replace('kCapacity = 512','kCapacity = 8192')
        }
        [pscustomobject]@{Path=$relative;BaselineSHA256=$before;Stage04SHA256=$after;Identical=($before -eq $after);AuthorizedHistoryCapacityOnly=$historyOnly}
    })
    $rows | Export-Csv local/stage04-source-integrity.csv -NoTypeInformation
    if (@($rows | Where-Object {!$_.Identical -and !$_.AuthorizedHistoryCapacityOnly}).Count) { throw 'Unauthorized original source difference from Stage03 baseline' }
    $staged = (Get-FileHash Dependencies/SRC/src/MultiChannelSrcEngine.cpp).Hash
    $compiled = (Get-FileHash local/stage04-ref/Stage3/native-reference/dependencies/SRC/src/MultiChannelSrcEngine.cpp).Hash
    if ($staged -ne $compiled) { throw 'Compiled nominal wire source is not byte-identical to staged source' }
    Write-Output "PASS $($rows.Count) source checks: $(@($rows | Where-Object Identical).Count) byte-identical, $(@($rows | Where-Object {!$_.Identical -and $_.AuthorizedHistoryCapacityOnly}).Count) exact authorized history-capacity change; native reference $native; nominal wire source identical"
} finally { Pop-Location }
