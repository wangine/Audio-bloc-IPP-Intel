cmake_minimum_required(VERSION 3.22)
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
endif()
project(EntropyDynamicRecovery VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_library(EntropyDynamicRecovery SHARED src/DynamicRecoveryModule.cpp)
target_include_directories(EntropyDynamicRecovery PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/R10ModuleABI/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(EntropyDynamicRecovery PRIVATE ENTROPY_MODULE_BUILD)
if(MSVC)
  target_compile_options(EntropyDynamicRecovery PRIVATE /fp:precise)
endif()
add_executable(DynamicRecoveryContract tests/DynamicRecoveryContract.cpp)
target_include_directories(DynamicRecoveryContract PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../Shared/R10ModuleABI/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_test(NAME DynamicRecoveryContract COMMAND DynamicRecoveryContract
  "${CMAKE_CURRENT_BINARY_DIR}/EntropyDynamicRecovery.dll")
