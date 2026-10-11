add_library(EntropyStagePublishV2 STATIC StagePublishV2.cpp)
target_compile_features(EntropyStagePublishV2 PUBLIC cxx_std_17)
target_include_directories(EntropyStagePublishV2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StageBusSDK/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../StageLink)
target_link_libraries(EntropyStagePublishV2 PUBLIC EntropyStageLink advapi32 ole32)
