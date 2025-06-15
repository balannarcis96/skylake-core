#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)

skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/assert-ut")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/logging-ut")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/core-info")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/skl-status")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/resources-dir")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/skl-vector")
