cmake_minimum_required(VERSION 3.24)
project(EntropyAudioCore VERSION 0.1.0 LANGUAGES CXX)

# This is an OFFLINE graph integration candidate, not the DAC runtime.
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "EntropyAudioCore requires Windows and MSVC x64")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(CTest)
option(ENTROPY_STAGE02_VALIDATION "Build isolated offline Stage02 validation tools" OFF)
option(ENTROPY_STAGE04_EGRESS "Build native egress and offline tests (never auto-start hardware)" OFF)
option(ENTROPY_STAGE05_CONTROL "Build native IPC control plane (physical output remains gated)" OFF)

set(IPP_ROOT "C:/Program Files (x86)/Intel/oneAPI/ipp/2026.0"
    CACHE PATH "Locally installed Intel IPP SDK, not distributed")
find_path(IPP_INCLUDE_DIR NAMES ipp.h PATHS "${IPP_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
find_library(IPP_SIGNAL_LIBRARY NAMES ippsmt PATHS "${IPP_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(IPP_VECTOR_LIBRARY NAMES ippvmmt PATHS "${IPP_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(IPP_CORE_LIBRARY NAMES ippcoremt PATHS "${IPP_ROOT}/lib" NO_DEFAULT_PATH REQUIRED)
message(STATUS "Intel IPP headers: ${IPP_INCLUDE_DIR}")

add_library(entropy_abi INTERFACE)
target_include_directories(entropy_abi INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Shared/include")

function(entropy_native_flags target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /utf-8 /fp:precise /EHsc)
endfunction()

# JUCE/Qt-free native C++ Crossover, *not* a plugin DLL.
add_library(entropy_crossover STATIC
    DSP/Crossover/src/FractionalDelay.cpp
    DSP/Crossover/src/PluginProcessor.cpp
    DSP/Crossover/src/NativeState.cpp)
target_include_directories(entropy_crossover PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/DSP/Crossover/include" "${IPP_INCLUDE_DIR}")
target_link_libraries(entropy_crossover PUBLIC entropy_abi
    "${IPP_SIGNAL_LIBRARY}" "${IPP_VECTOR_LIBRARY}" "${IPP_CORE_LIBRARY}")
entropy_native_flags(entropy_crossover)

add_library(entropy_output_matrix STATIC DSP/OutputMatrix/src/OutputMatrixEngine.cpp)
target_include_directories(entropy_output_matrix PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/DSP/OutputMatrix/include")
target_link_libraries(entropy_output_matrix PUBLIC entropy_abi)
entropy_native_flags(entropy_output_matrix)

add_library(entropy_headroom STATIC DSP/Headroom/src/HeadroomEngine.cpp)
target_include_directories(entropy_headroom PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/DSP/Headroom/include")
target_link_libraries(entropy_headroom PUBLIC entropy_abi)
entropy_native_flags(entropy_headroom)

# PCM24 is compiled and unit tested, not connected to the live output.
add_library(entropy_pcm24 STATIC
    Egress/PCM24/src/Pcm24Converter.cpp
    Egress/PCM24/src/Pcm24ModuleEngine.cpp)
target_include_directories(entropy_pcm24 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Egress/PCM24/include")
target_link_libraries(entropy_pcm24 PUBLIC entropy_abi)
entropy_native_flags(entropy_pcm24)

add_library(entropy_graph STATIC Engine/AudioGraph.cpp Engine/Telemetry.cpp Engine/BufferPool.cpp)
target_include_directories(entropy_graph PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(entropy_graph PUBLIC
    entropy_crossover entropy_output_matrix entropy_headroom entropy_abi)
entropy_native_flags(entropy_graph)

add_executable(EntropyAudioCore main.cpp)
target_link_libraries(EntropyAudioCore PRIVATE entropy_graph entropy_pcm24)
entropy_native_flags(EntropyAudioCore)
if(ENTROPY_STAGE04_EGRESS)
    include(cmake/Stage04.cmake)
endif()
if(ENTROPY_STAGE05_CONTROL)
    include(cmake/Stage05.cmake)
endif()

if(BUILD_TESTING)
    add_executable(EntropyThiranDelay Tests/ThiranDelayTests.cpp)
    target_link_libraries(EntropyThiranDelay PRIVATE entropy_crossover)
    entropy_native_flags(EntropyThiranDelay)
    add_test(NAME EntropyThiranDelay COMMAND EntropyThiranDelay)
    add_executable(EntropyThiranValidation Tests/ThiranValidation.cpp Tests/Stage02/RealtimeAllocationAudit.cpp Tests/Stage02/AuditExtras.cpp)
    target_link_libraries(EntropyThiranValidation PRIVATE entropy_crossover)
    entropy_native_flags(EntropyThiranValidation)
    add_test(NAME EntropyThiranValidation COMMAND EntropyThiranValidation "${CMAKE_BINARY_DIR}/thiran-results")
    set(THIRAN_BASELINE "${CMAKE_CURRENT_SOURCE_DIR}/local/thiran-baseline" CACHE PATH "Extracted immutable v0.0.3 Crossover for bit-exact comparison")
    if(EXISTS "${THIRAN_BASELINE}/DSP/Crossover/src/PluginProcessor.cpp")
        add_library(entropy_thiran_baseline STATIC Tests/ThiranBaselineBridge.cpp
            "${THIRAN_BASELINE}/DSP/Crossover/src/PluginProcessor.cpp"
            "${THIRAN_BASELINE}/DSP/Crossover/src/NativeState.cpp")
        target_compile_definitions(entropy_thiran_baseline PRIVATE entropy=thiran_baseline)
        target_include_directories(entropy_thiran_baseline PRIVATE "${THIRAN_BASELINE}/DSP/Crossover/include"
            "${THIRAN_BASELINE}/Shared/include" "${IPP_INCLUDE_DIR}")
        target_link_libraries(entropy_thiran_baseline PRIVATE "${IPP_SIGNAL_LIBRARY}" "${IPP_VECTOR_LIBRARY}" "${IPP_CORE_LIBRARY}")
        entropy_native_flags(entropy_thiran_baseline)
        target_link_libraries(EntropyThiranValidation PRIVATE entropy_thiran_baseline)
        target_compile_definitions(EntropyThiranValidation PRIVATE ENTROPY_THIRAN_BASELINE=1)
    endif()
    add_executable(EntropyStage04Fifo Tests/Stage04FifoTests.cpp)
    target_include_directories(EntropyStage04Fifo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    entropy_native_flags(EntropyStage04Fifo)
    add_test(NAME EntropyStage04Fifo COMMAND EntropyStage04Fifo)
    add_executable(EntropyStage04Policy Tests/Stage04PolicyTests.cpp)
    target_link_libraries(EntropyStage04Policy PRIVATE entropy_graph)
    entropy_native_flags(EntropyStage04Policy)
    add_test(NAME EntropyStage04Policy COMMAND EntropyStage04Policy)
    add_executable(EntropyStage03Reset Tests/Stage03ResetTests.cpp)
    target_link_libraries(EntropyStage03Reset PRIVATE entropy_crossover)
    entropy_native_flags(EntropyStage03Reset)
    add_test(NAME EntropyStage03Reset COMMAND EntropyStage03Reset)
    add_executable(EntropyOfflineConfig Tests/OfflineConfigTests.cpp)
    target_link_libraries(EntropyOfflineConfig PRIVATE entropy_graph)
    entropy_native_flags(EntropyOfflineConfig)
    add_test(NAME EntropyOfflineConfig COMMAND EntropyOfflineConfig)
    add_executable(EntropyGraphSmoke Tests/GraphSmoke.cpp)
    target_link_libraries(EntropyGraphSmoke PRIVATE entropy_graph)
    entropy_native_flags(EntropyGraphSmoke)
    add_test(NAME EntropyGraphSmoke COMMAND EntropyGraphSmoke)

    add_executable(EntropyMatrixUnit DSP/OutputMatrix/tests/OutputMatrixEngineTests.cpp)
    target_link_libraries(EntropyMatrixUnit PRIVATE entropy_output_matrix)
    entropy_native_flags(EntropyMatrixUnit)
    add_test(NAME EntropyMatrixUnit COMMAND EntropyMatrixUnit)

    add_executable(EntropyMatrixNoAlloc DSP/OutputMatrix/tests/OutputMatrixRealtimeAllocationTests.cpp)
    target_link_libraries(EntropyMatrixNoAlloc PRIVATE entropy_output_matrix)
    entropy_native_flags(EntropyMatrixNoAlloc)
    add_test(NAME EntropyMatrixNoAlloc COMMAND EntropyMatrixNoAlloc)

    add_executable(EntropyHeadroomUnit DSP/Headroom/tests/HeadroomEngineTests.cpp)
    target_link_libraries(EntropyHeadroomUnit PRIVATE entropy_headroom)
    entropy_native_flags(EntropyHeadroomUnit)
    add_test(NAME EntropyHeadroomUnit COMMAND EntropyHeadroomUnit)

    add_executable(EntropyHeadroomNoAlloc DSP/Headroom/tests/HeadroomRealtimeAllocationTests.cpp)
    target_link_libraries(EntropyHeadroomNoAlloc PRIVATE entropy_headroom)
    entropy_native_flags(EntropyHeadroomNoAlloc)
    add_test(NAME EntropyHeadroomNoAlloc COMMAND EntropyHeadroomNoAlloc)

    add_executable(EntropyPcm24Unit Egress/PCM24/tests/Pcm24ConverterTests.cpp)
    target_link_libraries(EntropyPcm24Unit PRIVATE entropy_pcm24)
    entropy_native_flags(EntropyPcm24Unit)
    add_test(NAME EntropyPcm24Unit COMMAND EntropyPcm24Unit)

    add_executable(EntropyPcm24EngineUnit Egress/PCM24/tests/Pcm24ModuleEngineTests.cpp)
    target_link_libraries(EntropyPcm24EngineUnit PRIVATE entropy_pcm24)
    entropy_native_flags(EntropyPcm24EngineUnit)
    add_test(NAME EntropyPcm24EngineUnit COMMAND EntropyPcm24EngineUnit)

    add_executable(EntropyIppFirUnit DSP/Crossover/tests/IppFirTests.cpp)
    target_link_libraries(EntropyIppFirUnit PRIVATE entropy_crossover)
    entropy_native_flags(EntropyIppFirUnit)
    add_test(NAME EntropyIppFirUnit COMMAND EntropyIppFirUnit)

    add_executable(EntropyNativeStateUnit DSP/Crossover/tests/NativeSupportTests.cpp)
    target_compile_definitions(EntropyNativeStateUnit PRIVATE ENTROPY_TEST_ENGINE=1)
    target_link_libraries(EntropyNativeStateUnit PRIVATE entropy_crossover)
    entropy_native_flags(EntropyNativeStateUnit)
    add_test(NAME EntropyNativeStateUnit COMMAND EntropyNativeStateUnit)

    # CMake Release normally defines NDEBUG. Keep assert-based regression
    # oracles ACTIVE in all test executables, even when DSP libs are optimized.
    foreach(test_target
        EntropyGraphSmoke EntropyMatrixUnit EntropyMatrixNoAlloc
        EntropyHeadroomUnit EntropyHeadroomNoAlloc
        EntropyPcm24Unit EntropyPcm24EngineUnit
        EntropyIppFirUnit EntropyNativeStateUnit)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    endforeach()

    add_test(NAME EntropyAudioCoreProbe COMMAND $<TARGET_FILE:EntropyAudioCore> --probe)
endif()

# STAGED NOT YET COMPILED: ClockSync requires nominal SRC / SoXR and r8brain,
# Guardian, WasapiSink, DeviceManager, IPC and separate DAC event-driven threads.
# Never launch physical DACs in Phase 01.

if(ENTROPY_STAGE02_VALIDATION)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "Stage02 requires BUILD_TESTING")
    endif()
    set(STAGE02_CROSSOVER_REFERENCE "${CMAKE_CURRENT_SOURCE_DIR}/local/ref-crossover/Stage3/native-reference/Modules/CrossoverIPP" CACHE PATH "Pristine pinned Crossover reference")
    set(STAGE02_NATIVE_REFERENCE "${CMAKE_CURRENT_SOURCE_DIR}/local/ref-native/Stage3/native-reference" CACHE PATH "Pristine pinned native reference")
    foreach(reference_file
        "${STAGE02_CROSSOVER_REFERENCE}/src/PluginProcessor.cpp"
        "${STAGE02_NATIVE_REFERENCE}/Modules/OutputMatrix/src/OutputMatrixEngine.cpp"
        "${STAGE02_NATIVE_REFERENCE}/Modules/HeadroomManager/src/HeadroomEngine.cpp")
        if(NOT EXISTS "${reference_file}")
            message(FATAL_ERROR "Missing verified reference: ${reference_file}")
        endif()
    endforeach()
    add_library(entropy_stage02_reference STATIC
        Tests/Stage02/ReferenceBridge.cpp
        "${STAGE02_CROSSOVER_REFERENCE}/src/PluginProcessor.cpp"
        "${STAGE02_CROSSOVER_REFERENCE}/src/NativeState.cpp"
        "${STAGE02_NATIVE_REFERENCE}/Modules/OutputMatrix/src/OutputMatrixEngine.cpp"
        "${STAGE02_NATIVE_REFERENCE}/Modules/HeadroomManager/src/HeadroomEngine.cpp")
    target_compile_definitions(entropy_stage02_reference PRIVATE entropy=stage02_reference)
    target_include_directories(entropy_stage02_reference PRIVATE
        "${STAGE02_CROSSOVER_REFERENCE}/include"
        "${STAGE02_NATIVE_REFERENCE}/Modules/OutputMatrix/include"
        "${STAGE02_NATIVE_REFERENCE}/Modules/HeadroomManager/include"
        "${STAGE02_NATIVE_REFERENCE}/Shared/include" "${IPP_INCLUDE_DIR}")
    target_link_libraries(entropy_stage02_reference PRIVATE "${IPP_SIGNAL_LIBRARY}" "${IPP_VECTOR_LIBRARY}" "${IPP_CORE_LIBRARY}")
    entropy_native_flags(entropy_stage02_reference)
    add_executable(EntropyStage02 Tests/Stage02/Validation.cpp
        Tests/Stage02/RealtimeAllocationAudit.cpp Tests/Stage02/AuditExtras.cpp)
    target_link_libraries(EntropyStage02 PRIVATE entropy_graph entropy_stage02_reference)
    entropy_native_flags(EntropyStage02)
    add_executable(EntropyStage03Integration Tests/Stage03IntegrationTests.cpp)
    target_link_libraries(EntropyStage03Integration PRIVATE entropy_graph entropy_stage02_reference)
    entropy_native_flags(EntropyStage03Integration)
    add_test(NAME EntropyStage03Integration COMMAND EntropyStage03Integration)
    add_executable(EntropyStage03Fade Tests/Stage03FadeBenchmark.cpp)
    target_link_libraries(EntropyStage03Fade PRIVATE entropy_graph)
    entropy_native_flags(EntropyStage03Fade)
    set(STAGE02_RESULTS "${CMAKE_BINARY_DIR}/stage02-results")
    file(MAKE_DIRECTORY "${STAGE02_RESULTS}")
    add_test(NAME EntropyStage02Identity COMMAND EntropyStage02 --identity "${STAGE02_RESULTS}")
    add_test(NAME EntropyStage02Realtime COMMAND EntropyStage02 --realtime "${STAGE02_RESULTS}")
endif()
