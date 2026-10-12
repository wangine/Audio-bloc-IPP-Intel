cmake_minimum_required(VERSION 3.22)
project(EntropyR8BrainStaging LANGUAGES NONE)
file(GLOB_RECURSE R8BRAIN_SOURCE CONFIGURE_DEPENDS src/r8brain-free-src/*)
add_custom_target(EntropyR8BrainStaging SOURCES ${R8BRAIN_SOURCE})

