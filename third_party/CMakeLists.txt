#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)

set(FMT_SYSTEM_HEADERS OFF)
set(FMT_INSTALL OFF)
add_subdirectory("fmt")
target_link_libraries(fmt PUBLIC -lc++ -lc++abi)
target_compile_definitions(fmt PUBLIC FMT_USE_FULL_CACHE_DRAGONBOX=1)

set(SKL_MIMALLOC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mimalloc" CACHE STRING "[MiMalloc path]" FORCE)
set(SKL_MAGICENUM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/magic_enum" CACHE STRING "[MagicEnum path]" FORCE)
