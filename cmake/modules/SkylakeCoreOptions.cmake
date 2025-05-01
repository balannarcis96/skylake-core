#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

include(SkylakeCoreUtils)
include(SkylakeCoreWarnings)
include(SkylakeCoreOptionValues)

# Build type
set(SKL_BUILD_TYPE "DEV" CACHE STRING "[CORE] Build type")

# Misc
set(SKL_CORE_ENABLE_SANITIZATION ON CACHE BOOL "[DEV/STAGING] Enable address sanitization")
set(SKL_CORE_ENABLE_TESTS ON CACHE BOOL "[DEV/STAGING & TopLevel] Enable tests")
set(SKL_CORE_ADD_PRESETS ON CACHE BOOL "Add core presets")
set(SKL_CORE_NO_EXCEPTIONS OFF CACHE BOOL "Disable exceptions support")

# Set properties options
set_property(CACHE SKL_BUILD_TYPE PROPERTY STRINGS ${SKL_CORE_BUILD_TYPE_OPTIONS}) # Build type

if("${SKL_BUILD_TYPE}" STREQUAL "SHIPPING")
    if((NOT CMAKE_BUILD_TYPE STREQUAL "Release") AND (NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        message(FATAL_ERROR "[SHIPPING] Must be built in Release or RelWithDebInfo type")
    endif()
    set(SKL_CORE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
endif()
if(NOT PROJECT_IS_TOP_LEVEL)
    set(SKL_CORE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
endif()
