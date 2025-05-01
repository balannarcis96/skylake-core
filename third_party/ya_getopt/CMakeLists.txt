#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)
project(ya_getopt VERSION 1.0.0)

add_library(ya_getopt STATIC "ya_getopt.cpp" "ya_getopt.h")

target_include_directories(ya_getopt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")
