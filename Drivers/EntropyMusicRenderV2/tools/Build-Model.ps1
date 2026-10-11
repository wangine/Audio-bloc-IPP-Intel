param([string]$Configuration='Release')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'build/model'
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere='C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if(!$vs){throw 'MSVC unavailable'}
$cmake='C:/Qt/Tools/CMake_64/bin/cmake.exe'
$ninja='C:/Qt/Tools/Ninja/ninja.exe'
$batch=Join-Path $out 'build.cmd'
@"
@echo off
call "$vs/VC/Auxiliary/Build/vcvars64.bat" > nul
if errorlevel 1 exit /b 1
"$cmake" -S "$root" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=$Configuration -DCMAKE_MAKE_PROGRAM="$ninja" -DPython3_EXECUTABLE="C:/Users/bobi/AppData/Local/Programs/Python/Python39/python.exe"
if errorlevel 1 exit /b 1
"$cmake" --build "$out"
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $batch -Encoding ascii
& cmd.exe /d /c "`"$batch`"" *> (Join-Path $out 'build.log')
$code=$LASTEXITCODE
Get-Content -LiteralPath (Join-Path $out 'build.log') -Tail 15
if($code){throw "User-mode build failed ($code)"}
