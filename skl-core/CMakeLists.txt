cmake_minimum_required(VERSION 3.8)

# Skylake Core Lib
set(SKL_CORE_LIB_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "libskl-core root dir" FORCE)
file(GLOB_RECURSE _SKL_CORE_SOURCE_FILES "${SKL_CORE_LIB_SRC_ROOT}/source/*.cpp" "${SKL_CORE_LIB_SRC_ROOT}/source/*.h" "${SKL_CORE_LIB_SRC_ROOT}/source/*.hpp")
file(GLOB_RECURSE _SKL_CORE_PUBLIC_FILES "${SKL_CORE_LIB_SRC_ROOT}/include/*/*")

set(SKL_CORE_LIB_SOURCE_FILES "${_SKL_CORE_SOURCE_FILES}" CACHE STRING "libskl-core source files" FORCE)
set(SKL_CORE_LIB_PUBLIC_FILES "${_SKL_CORE_PUBLIC_FILES}" CACHE STRING "libskl-core public include files" FORCE)
