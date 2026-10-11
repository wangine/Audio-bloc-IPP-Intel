from pathlib import Path
root = Path(__file__).absolute().parent.parent
original = root.parent / 'Audio-bloc-IPP-Intel/Drivers/EntropyMusicRenderV2/tools/Run-Live-Acceptance.ps1'
if not original.exists():
    original = root.parents[2] / 'Drivers/EntropyMusicRenderV2/tools/Run-Live-Acceptance.ps1'
if not original.exists():
    original = root / 'ThirdParty/DriverAudit/Drivers/EntropyMusicRenderV2/tools/Run-Live-Acceptance.ps1'
text = original.read_text(encoding='utf-8-sig')
text = text.replace('[string]$Repository)', '[string]$Repository,[Parameter(Mandatory=$true)][string]$CoreCandidate,[Parameter(Mandatory=$true)][string]$CandidateSha256)')
text = text.replace(". (Join-Path $PSScriptRoot 'Assert-Package.ps1')", ". (Join-Path $Package 'tools/Assert-Package.ps1')")
text = text.replace("if($manifestRunner.Count -ne 1 -or $runnerHash -ne $manifestRunner[0].sha256){throw 'Runner differs from the manifested package; invoke the packaged tools/Run-Live-Acceptance.ps1'}", "# Diagnostic runner is separately recorded; it is not the frozen r6 qualification runner.")
text = text.replace("if((Get-FileHash -LiteralPath $frozen).Hash -ne $manifest.frozenCoreSha256){throw 'Core differs from the frozen package reference'}", "if((Get-FileHash -LiteralPath $frozen).Hash -ne $manifest.frozenCoreSha256){throw 'Frozen reference changed'}\n$frozen=[IO.Path]::GetFullPath($CoreCandidate)\nif((Get-FileHash -LiteralPath $frozen).Hash -ne $CandidateSha256){throw 'Diagnostic candidate hash differs'}")
text = text.replace("acceptance='pending; no model result promoted to kernel acceptance'", "scope='DIAGNOSTIC_CANDIDATE_ONLY';acceptance='not frozen Core qualification';candidateSha256=$CandidateSha256")
text = text.replace("frozenCoreSha256=(Get-FileHash -LiteralPath $frozen).Hash", "frozenCoreSha256=$manifest.frozenCoreSha256")
text = text.replace("$hashes.coreSha256 -ne $hashes.frozenCoreSha256", "$hashes.coreSha256 -ne $CandidateSha256")
text = text.replace("Frozen Core copy differs", "Candidate Core copy differs")
text = text.replace("exact frozen Core", "exact candidate Core")
text = text.replace("$previousPipe=$env:ENTROPY_MUSIC_AUDIT_PIPE", "$previousPipe=$env:ENTROPY_MUSIC_AUDIT_PIPE;$previousTrace=$env:ENTROPY_STAGE1_TRACE_DIRECTORY")
text = text.replace("$coreProcess=Start-Process", "$env:ENTROPY_STAGE1_TRACE_DIRECTORY=Join-Path $folder 'timing'\n        $coreProcess=Start-Process")
text = text.replace("$env:ENTROPY_MUSIC_AUDIT_PIPE=$previousPipe", "$env:ENTROPY_MUSIC_AUDIT_PIPE=$previousPipe;$env:ENTROPY_STAGE1_TRACE_DIRECTORY=$previousTrace")
(root / 'tools/Run-Diagnostic-Candidate.ps1').write_text(text, encoding='utf-8-sig')
