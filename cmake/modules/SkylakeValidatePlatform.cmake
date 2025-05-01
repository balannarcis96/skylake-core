#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

function(SkylakeValidatePlatform)
    # OS
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME} (requires Linux)")
    endif()

    # Arch
    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR} (requires x86_64)")
    endif()

    # Compiler ID
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID} (requires Clang)")
    endif()

    # Clang ≥ 19
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.0")
        message(FATAL_ERROR "Clang ${CMAKE_CXX_COMPILER_VERSION} is too old (requires ≥ 19)")
    endif()

    # C++23+
    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard" FORCE)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    if(CMAKE_CXX_STANDARD LESS 23)
        message(FATAL_ERROR "C++ standard ${CMAKE_CXX_STANDARD} is too low (requires ≥ 23)")
    endif()
endfunction()

SkylakeValidatePlatform()
