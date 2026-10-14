cmake_minimum_required(VERSION 3.22)
project(EntropyPcmFrontierOfflineReference LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STAGE1_ROOT "D:/Projects/3Stage-3/Stage1-v0003/delivery/r7-verified-local/stage1-native-music-v0003-r7-source")
add_executable(offline_ref
    offline_ref.cpp
    "${STAGE1_ROOT}/ThirdParty/NativeSrc/Modules/SRC/src/MultiChannelSrcEngine.cpp")
target_include_directories(offline_ref PRIVATE
    "${STAGE1_ROOT}/ThirdParty/NativeSrc/Modules/SRC/include"
    "${STAGE1_ROOT}/ThirdParty/NativeSrc/Shared/ModuleABI/include")
target_compile_definitions(offline_ref PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(offline_ref PRIVATE /utf-8 /W4 /fp:precise)
endif()
