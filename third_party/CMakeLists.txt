#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)

option(FMT_INSTALL "Generate the install target." OFF)
add_compile_definitions(FMT_EXCEPTIONS=0)
add_subdirectory("fmt")

add_subdirectory("ya_getopt")

if(SKL_ADD_GTEST_GMOCK)
    add_subdirectory("googletest-1.14.0")
endif()

set(SKL_MIMALLOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mimalloc" CACHE STRING "[MiMalloc path]" FORCE)
set(SKL_MAGICENUM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/magic_enum" CACHE STRING "[MagicEnum path]" FORCE)
