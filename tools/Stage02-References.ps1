param([switch]$VerifyOnly, [string]$ImportedRoot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (!$ImportedRoot) { $ImportedRoot=$root }
$ImportedRoot=(Resolve-Path -LiteralPath $ImportedRoot).Path
$local = Join-Path $root 'local'
$repo = Join-Path $local 'reference-repo'
if (!(Test-Path -LiteralPath $repo)) {
    if ($VerifyOnly) { throw 'Reference checkout missing' }
    & git clone --no-checkout https://github.com/wangine/RenderEngine-Stage-3.git $repo
    if ($LASTEXITCODE) { throw 'Reference clone failed' }
}
$references = @(
    @{ Revision='99b84d13a6808486690e24feba35128d7675be95'; Folder='ref-crossover'; Trees=@('Stage3/native-reference/Modules/CrossoverIPP') },
    @{ Revision='6cbac37233c8094e39828de8309a9fe13d724ad4'; Folder='ref-native'; Trees=@('Stage3/native-reference/Modules', 'Stage3/native-reference/Shared') },
    @{ Revision='2d5b35165de2ac6be5afa9420851fc24895c6416'; Folder='ref-v07'; Trees=@('releases/crossover/V07', 'docs/CROSSOVER-V07-VALIDATION.md') }
)
foreach ($reference in $references) {
    $folder = Join-Path $local $reference.Folder
    if (!(Test-Path -LiteralPath $folder)) {
        if ($VerifyOnly) { if ($reference.Folder -eq 'ref-v07') { Write-Warning 'BLOCKED: optional V07 reference unavailable'; continue }; throw "Reference extraction missing: $folder" }
        New-Item -ItemType Directory -Path $folder | Out-Null
        $archive = Join-Path $local ($reference.Folder + '.tar')
        & git -C $repo archive --format=tar "--output=$archive" $reference.Revision @($reference.Trees)
        if ($LASTEXITCODE) { if ($reference.Folder -eq 'ref-v07') { Write-Warning 'BLOCKED: optional V07 archive unavailable'; continue }; throw 'Reference archive failed' }
        & tar -xf $archive -C $folder
        if ($LASTEXITCODE) { if ($reference.Folder -eq 'ref-v07') { Write-Warning 'BLOCKED: optional V07 extraction failed'; continue }; throw 'Reference extraction failed' }
    }
}
$crossover = Join-Path $local 'ref-crossover/Stage3/native-reference/Modules/CrossoverIPP'
$native = Join-Path $local 'ref-native/Stage3/native-reference'
$files = @()
foreach ($item in @(
    @{ Imported='DSP/Crossover'; Original=$crossover; Revision=$references[0].Revision },
    @{ Imported='DSP/OutputMatrix'; Original=(Join-Path $native 'Modules/OutputMatrix'); Revision=$references[1].Revision },
    @{ Imported='DSP/Headroom'; Original=(Join-Path $native 'Modules/HeadroomManager'); Revision=$references[1].Revision }
)) {
    foreach ($subfolder in @('include','src')) {
        $importedFolder = Join-Path $ImportedRoot ($item.Imported + '/' + $subfolder)
        foreach ($file in Get-ChildItem -LiteralPath $importedFolder -File -Recurse) {
            $relative = $file.FullName.Substring((Join-Path $ImportedRoot $item.Imported).Length + 1)
            $original = Join-Path $item.Original $relative
            if (!(Test-Path -LiteralPath $original)) { throw "Original source missing: $original" }
            $a=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
            $b=(Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash
            # Git checkouts may normalize line endings; preserve byte hashes and
            # compare canonical UTF-8/LF text separately without editing files.
            $textA=[IO.File]::ReadAllText($file.FullName).Replace("`r`n","`n")
            $textB=[IO.File]::ReadAllText($original).Replace("`r`n","`n")
            $equal=$textA -ceq $textB
            $files += [pscustomobject]@{File=($item.Imported+'/'+$relative);Revision=$item.Revision;SHA256=$a;ReferenceSHA256=$b;ByteIdentical=($a -eq $b);TextIdentical=$equal}
            if (!$equal) { throw "Pinned source differs: $($file.FullName)" }
        }
    }
}
$gold = Join-Path $local 'ref-v07/releases/crossover/V07/EntropyCrossover.dll'
$goldHash = 'BLOCKED: unavailable'
if (Test-Path -LiteralPath $gold) {
    $goldHash = (Get-FileHash -LiteralPath $gold -Algorithm SHA256).Hash
    if ($goldHash -ne '1B0F76BF38F2E387945CF16A8DE38B1AE8FA25A9A9AF2AC976473671D0A7889A') { Write-Warning 'BLOCKED: V07 golden DLL hash mismatch; runner must exclude it' }
} else { Write-Warning 'BLOCKED: optional V07 golden DLL missing; native checks can continue' }
$resultFolder = Join-Path $root 'artifacts/stage02'
New-Item -ItemType Directory -Force -Path $resultFolder | Out-Null
$files | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $resultFolder 'source-provenance.json') -Encoding utf8
Write-Host "PASS: $($files.Count) pinned source files; V07 SHA256=$goldHash"
