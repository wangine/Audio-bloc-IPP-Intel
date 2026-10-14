include(FetchContent)
set(ENTROPY_LIVE_WDL_COMMIT "2b260b915f8ca7cb7859b9c0b7ba7e48b166f0af")
FetchContent_Declare(entropy_live_wdl_src
 GIT_REPOSITORY https://github.com/justinfrankel/WDL.git
 GIT_TAG ${ENTROPY_LIVE_WDL_COMMIT}
 GIT_SHALLOW FALSE
 SOURCE_SUBDIR entropy_no_cmake_subproject)
FetchContent_MakeAvailable(entropy_live_wdl_src)
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${entropy_live_wdl_src_SOURCE_DIR}" rev-parse HEAD
 OUTPUT_VARIABLE wdl_head OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE wdl_status)
if(NOT wdl_status EQUAL 0 OR NOT wdl_head STREQUAL ENTROPY_LIVE_WDL_COMMIT)
 message(FATAL_ERROR "Unexpected WDL source revision")
endif()
file(READ "${entropy_live_wdl_src_SOURCE_DIR}/WDL/resample.cpp" wdl_code)
string(REPLACE "\r\n" "\n" wdl_code "${wdl_code}")
set(old_cutoff "  double filtpos = 1.0;\n  if (m_ratio > 1.0) filtpos = 1.0 / (m_ratio * 1.03);")
set(old_ideal "  if (wantinterp)\n  {")
string(FIND "${wdl_code}" "${old_cutoff}" cutoff_at)
string(FIND "${wdl_code}" "${old_ideal}" ideal_at)
if(cutoff_at LESS 0 OR ideal_at LESS 0)
 message(FATAL_ERROR "Pinned WDL adaptation anchors differ; do not silently change vendor source")
endif()
# Explicitly altered source. Constant bandwidth/table for ratios 0.999..1.001.
# All sample interpolation/history is the upstream Cockos implementation.
string(REPLACE "${old_cutoff}" "  const double filtpos = 0.97; // Entropy: fixed near-unity bank, prepared before ARM" wdl_code "${wdl_code}")
string(REPLACE "${old_ideal}" "  if (false && wantinterp) // Entropy: never change phase-table geometry at runtime\n  {" wdl_code "${wdl_code}")
set(wdl_generated "${CMAKE_CURRENT_BINARY_DIR}/EntropyFixedWdlResample.cpp")
file(WRITE "${wdl_generated}" "// ALTERED Cockos WDL ${ENTROPY_LIVE_WDL_COMMIT}: fixed cutoff and phase-bank geometry.\n${wdl_code}")
add_library(entropy_live_wdl STATIC "${wdl_generated}")
target_include_directories(entropy_live_wdl PUBLIC "${entropy_live_wdl_src_SOURCE_DIR}/WDL")
target_compile_definitions(entropy_live_wdl PUBLIC WDL_RESAMPLE_FULL_SINC_PRECISION WDL_RESAMPLE_TYPE=double)
target_compile_features(entropy_live_wdl PUBLIC cxx_std_20)
if(MSVC)
 target_compile_definitions(entropy_live_wdl PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
 target_compile_options(entropy_live_wdl PRIVATE /utf-8 /fp:precise /EHsc)
else()
 target_compile_options(entropy_live_wdl PRIVATE -ffp-contract=off)
endif()
