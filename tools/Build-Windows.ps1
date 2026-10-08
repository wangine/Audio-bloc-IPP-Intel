param(
    [string]$IppRoot = "C:\Program Files (x86)\Intel\oneAPI\ipp\2026.0",
    [string]$BuildFolder = "build\msvc-release"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $root $BuildFolder
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
$vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw "vswhere.exe missing: install MSVC x64 tools" }
$vsroot = (& $vswhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (!$vsroot) { throw "No MSVC x64 Visual Studio installation found" }
$vcvars = Join-Path $vsroot "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvars)) { throw "Missing vcvars64.bat: $vcvars" }

$cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { "C:\Qt\Tools\CMake_64\bin\cmake.exe" }
if (!(Test-Path $cmake)) { throw "cmake.exe missing" }
if (!(Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
    if (Test-Path "C:\Qt\Tools\Ninja\ninja.exe") {
        $env:PATH = "C:\Qt\Tools\Ninja;" + $env:PATH
    } else { throw "ninja.exe missing" }
}
if (!(Test-Path (Join-Path $IppRoot "include\ipp.h"))) {
    throw "Intel oneAPI IPP SDK missing: $IppRoot"
}
$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
if (!(Test-Path $ctest)) { $ctest = "ctest.exe" }

$q = [char]34
$commands = New-Object System.Collections.Generic.List[string]
$commands.Add('@echo off')
$commands.Add('call ' + $q + $vcvars + $q + ' || exit /b 1')
$commands.Add($q + $cmake + $q + ' -S ' + $q + $root + $q + ' -B ' + $q + $buildDir + $q + ' -G Ninja -DCMAKE_BUILD_TYPE=Release ' + $q + '-DIPP_ROOT=' + $IppRoot + $q + ' -DBUILD_TESTING=ON || exit /b 1')
$commands.Add($q + $cmake + $q + ' --build ' + $q + $buildDir + $q + ' --parallel 4 || exit /b 1')
$commands.Add($q + $ctest + $q + ' --test-dir ' + $q + $buildDir + $q + ' --output-on-failure || exit /b 1')
$commands.Add($q + (Join-Path $buildDir 'EntropyAudioCore.exe') + $q + ' --probe || exit /b 1')
$batch = Join-Path $buildDir "run-msvc-ipp.cmd"
Set-Content -LiteralPath $batch -Value $commands -Encoding ascii
Write-Host "AudioCore: compile MSVC x64 + static Intel IPP (OFFLINE)"
& cmd.exe /d /c ($q + $batch + $q)
if ($LASTEXITCODE -ne 0) { throw "Compile/CTest failed with exit code $LASTEXITCODE" }
Write-Host "PASS: native MSVC build and offline tests. No DAC/IPC started."
