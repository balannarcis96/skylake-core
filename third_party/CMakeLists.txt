#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)

set(FMT_SYSTEM_HEADERS OFF)
set(FMT_INSTALL OFF)
add_subdirectory("fmt")
target_link_libraries(fmt PUBLIC -lc++ -lc++abi)

add_subdirectory("ya_getopt")

if(SKL_ADD_GTEST_GMOCK)
    add_subdirectory("googletest-1.14.0")
endif()

set(SKL_MIMALLOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mimalloc" CACHE STRING "[MiMalloc path]" FORCE)
set(SKL_MAGICENUM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/magic_enum" CACHE STRING "[MagicEnum path]" FORCE)
