cmake_minimum_required(VERSION 3.24)
project(EntropyProgramControlContracts LANGUAGES CXX)
if(NOT WIN32)
 message(FATAL_ERROR "Real ProgramIngress contracts require Windows; no physical devices are used")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory("${ROOT}/Transport/StageBusSDK" stagebus)
add_subdirectory("${ROOT}/Transport/StageLink" stagelink)
add_subdirectory("${ROOT}/Transport/StagePublishV2" publish)
add_executable(EntropyProgramControlContracts ProgramControlContracts.cpp
 "${ROOT}/Engine/Input/InputContract.cpp"
 "${ROOT}/Engine/Input/DomainIngress.cpp"
 "${ROOT}/Engine/Input/ProgramIngress.cpp"
 "${ROOT}/Engine/Input/ProgramSelection.cpp")
target_include_directories(EntropyProgramControlContracts PRIVATE "${ROOT}" "${ROOT}/Shared/include")
target_link_libraries(EntropyProgramControlContracts PRIVATE EntropyStagePublishV2 avrt)
target_compile_options(EntropyProgramControlContracts PRIVATE /W4 /EHsc /fp:precise /utf-8)
