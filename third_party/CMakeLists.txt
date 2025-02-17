cmake_minimum_required (VERSION 3.8)

option(FMT_INSTALL "Generate the install target." OFF)
add_compile_definitions(FMT_EXCEPTIONS=0)
add_subdirectory("fmt-10.1.1")

if(SKL_ADD_GTEST_GMOCK)
    add_subdirectory("googletest-1.14.0")
endif()

set(SKL_MIMALLOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mimalloc-2.1.7" CACHE STRING "[MiMalloc path]" FORCE)
set(SKL_MAGICENUM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/magic_enum" CACHE STRING "[MagicEnum path]" FORCE)
