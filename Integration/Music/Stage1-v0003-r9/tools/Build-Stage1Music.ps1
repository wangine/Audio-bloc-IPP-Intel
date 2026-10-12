param([switch]$SkipTests)
$ErrorActionPreference='Stop'
$stage1ProjectRoot=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Path (Join-Path $stage1ProjectRoot 'evidence') -Force | Out-Null
$stage1VsWhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$stage1VsPath=(& $stage1VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if(!$stage1VsPath){throw 'MSVC x64 installation unavailable'}
$stage1Vars=Join-Path $stage1VsPath 'VC\Auxiliary\Build\vcvars64.bat'
$stage1EnvironmentScript=Join-Path $stage1ProjectRoot 'evidence\msvc-environment.cmd'
@("@call `"$stage1Vars`" >nul",'@set') | Set-Content -LiteralPath $stage1EnvironmentScript -Encoding ascii
$stage1Environment=& $env:ComSpec /d /c $stage1EnvironmentScript
if($LASTEXITCODE){throw 'MSVC environment initialization failed'}
foreach($stage1Line in $stage1Environment){if($stage1Line -match '^([^=]+)=(.*)$'){Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]}}
$stage1Cmake='C:\Qt\Tools\CMake_64\bin\cmake.exe'
$stage1Ninja='C:\Qt\Tools\Ninja\ninja.exe'
& $stage1Cmake -S $stage1ProjectRoot -B "$stage1ProjectRoot\build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$stage1Ninja" -DCMAKE_BUILD_TYPE=Release 2>&1 | Tee-Object "$stage1ProjectRoot\evidence\configure.txt"
if($LASTEXITCODE){throw 'Configure failed'}
& $stage1Cmake --build "$stage1ProjectRoot\build" --parallel 4 *> "$stage1ProjectRoot\evidence\build.txt"
$stage1BuildCode=$LASTEXITCODE
Get-Content -LiteralPath "$stage1ProjectRoot\evidence\build.txt" -Tail 20
if($stage1BuildCode){throw 'Build failed'}
if(!$SkipTests){& (Join-Path (Split-Path $stage1Cmake) 'ctest.exe') --test-dir "$stage1ProjectRoot\build" --output-on-failure;if($LASTEXITCODE){throw 'Acceptance failed'}}
