param([string]$QtRoot='C:/Qt/6.11.2/mingw_64', [string]$MingwRoot='C:/Qt/Tools/mingw1310_64', [string]$OriginalRuntime='D:/Projects/Entropy Codex Lucru/Modular 3 stages/Etapa 2 DSP/versions/v0006-parametric-eq')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source=Join-Path $root 'Integration/Stage2/source';$build=Join-Path $root 'local/stage010/stage2-build'
$env:PATH="$MingwRoot/bin;C:/Qt/Tools/Ninja;$QtRoot/bin;"+$env:PATH
$moduleRoot=Join-Path $root 'Integration/Stage2/runtime/stage2/modules';New-Item -ItemType Directory -Path $moduleRoot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $OriginalRuntime 'modules') -Destination (Split-Path $moduleRoot) -Recurse -Force
$cmake='C:/Qt/Tools/CMake_64/bin/cmake.exe'
& $cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$MingwRoot/bin/g++.exe" "-DCMAKE_C_COMPILER=$MingwRoot/bin/gcc.exe"
if($LASTEXITCODE){throw 'Stage2 configure failed'}
& $cmake --build $build --parallel 4 --target EntropyDspEngine DspBypassIdentityTests DspBusIntegrationTests DspAllocationTests DspLocalDemandTests DspSourceIsolationTests DspPublishedOutputTests DspCenterNativeTests DspCenterRouteTests DspAlignmentTests DspLrcRouteTests DspEqRouteTests DspEqNativeTests
if($LASTEXITCODE){throw 'Stage2 compile failed'}
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir $build -R '^Dsp(BypassIdentity|BusIntegration|Allocation|LocalDemand|SourceIsolation|PublishedOutput|CenterNative|CenterRoute|Alignment|LrcRoute|EqRoute|EqNative)$' --output-on-failure
if($LASTEXITCODE){throw 'Stage2 tests failed'}
