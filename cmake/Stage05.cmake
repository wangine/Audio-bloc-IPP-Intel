# Original Stage05 target declarations retained verbatim in Stage05BaseTargets.
# The physical LiveSinkWorkers implementation in this branch is the new renderer.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Stage05BaseTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/RendererLive/Integration.cmake")
