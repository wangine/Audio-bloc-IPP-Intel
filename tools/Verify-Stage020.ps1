param(
 [Parameter(Mandatory=$true)][string]$Profile,
 [Parameter(Mandatory=$true)][string]$Fragment,
 [Parameter(Mandatory=$true)][string]$ActiveExport,
 [string]$BuildDirectory='build/msvc-thiran',
 [string]$OutputDirectory='local/stage020-reproduction',
 [string]$Python='C:/Users/bobi/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=[System.IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$output=[System.IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if(!(Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))){throw 'Configure the existing MSVC/IPP Stage05 build first; this bounded script builds only the offline harness.'}
if(Test-Path -LiteralPath $output){throw 'Use a fresh output directory; existing evidence is preserved.'}
foreach($file in @($Profile,$Fragment,$ActiveExport,$Python)){if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing input: $file"}}
New-Item -ItemType Directory -Path $output | Out-Null
$runner=Join-Path $output 'build.cmd'
$cmd=@('@echo off','call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1','set "PATH=C:\Qt\Tools\Ninja;%PATH%"',('"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "'+$build+'" --target EntropyStage020Transfer EntropyStage020TapAccess --parallel 2 || exit /b 1'))
Set-Content -LiteralPath $runner -Value $cmd -Encoding ascii
& $runner *> (Join-Path $output 'build.txt')
if($LASTEXITCODE){throw 'Offline harness build failed'}
& (Join-Path $build 'EntropyStage020TapAccess.exe')
if($LASTEXITCODE){throw 'Tap integration failed'}
& (Join-Path $build 'EntropyStage020Transfer.exe') $Profile $Fragment $ActiveExport (Join-Path $output 'replay') *> (Join-Path $output 'replay.txt')
if($LASTEXITCODE){throw 'Bounded Graph/ASRC replay failed; inspect replay.txt'}
& $Python (Join-Path $root 'tools/Analyze-Stage020.py') (Join-Path $output 'replay') $ActiveExport $Profile (Join-Path $output 'report') *> (Join-Path $output 'analysis.txt')
if($LASTEXITCODE){throw 'Independent transfer comparison failed; inspect analysis.txt'}
Write-Output "Offline evidence: $output. No hardware/source opened. Diagnostic profile is not an acoustic recommendation."
