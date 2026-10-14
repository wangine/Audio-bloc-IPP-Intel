include("${CMAKE_CURRENT_SOURCE_DIR}/RendererLive/Wdl.cmake")
target_sources(entropy_control_runtime PRIVATE
 "${CMAKE_CURRENT_SOURCE_DIR}/RendererLive/WasapiOutput.cpp"
 "${CMAKE_CURRENT_SOURCE_DIR}/RendererLive/DevicePreflight.cpp")
target_link_libraries(entropy_control_runtime PRIVATE entropy_live_wdl ole32 uuid avrt bcrypt)
target_compile_definitions(entropy_control_runtime PRIVATE ENTROPY_LIVE_RENDERER_V1=1)
message(STATUS "Stage3 physical output: LiveV1, FP64 WDL, prepared PCM, independent WASAPI owners")
