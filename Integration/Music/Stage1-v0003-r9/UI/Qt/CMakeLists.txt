cmake_minimum_required(VERSION 3.22)
project(EntropyStage1Qt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
add_executable(EntropyMusicSourceQt WIN32 main.cpp)
target_include_directories(EntropyMusicSourceQt PRIVATE ../../ABI)
target_compile_definitions(EntropyMusicSourceQt PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(EntropyMusicSourceQt PRIVATE Qt6::Core Qt6::Widgets advapi32)

