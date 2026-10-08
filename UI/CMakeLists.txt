cmake_minimum_required(VERSION 3.24)
project(EntropyUI LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
add_library(entropy_ui STATIC Client/IpcClient.cpp Client/EngineFacade.cpp
 Legacy/RenderWindow.cpp Legacy/RenderOverview.cpp Legacy/RenderUiBridge.cpp
 Legacy/R10/CrossoverPage.cpp Legacy/R10/OutputMatrixPage.cpp Legacy/R10/HeadroomPage.cpp
 Legacy/R10/DeviceManagerPage.cpp Legacy/R10/OutputStatusSemantics.cpp)
target_include_directories(entropy_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/Legacy" "${CMAKE_CURRENT_SOURCE_DIR}/Contracts" "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/include")
target_link_libraries(entropy_ui PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
target_compile_definitions(entropy_ui PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
add_executable(EntropyUI main.cpp)
target_link_libraries(EntropyUI PRIVATE entropy_ui)
add_executable(EntropyStage05Ui ../Tests/Stage05UiTests.cpp)
target_link_libraries(EntropyStage05Ui PRIVATE entropy_ui)
