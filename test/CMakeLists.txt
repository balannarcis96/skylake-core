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
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/spsc-bidirectional-ring")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/spsc-ring")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/stable-object-pool")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/static-bit-set")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/dynamic-bit-set")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/timer-wheel")
skl_AddCoreTest("${CMAKE_CURRENT_SOURCE_DIR}/deck")
