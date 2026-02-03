#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
# Skylake Core Logging Configuration
#
include_guard()

# Function to configure log level mask for a specific build type
# Usage: skl_config_log_by_build_type(<BUILD_TYPE> <LOG_LEVEL>...)
# Example: skl_config_log_by_build_type(DEV FATAL ERROR WARNING INFO DEBUG TRACE)
# Example: skl_config_log_by_build_type(SHIPPING FATAL ERROR)
function(skl_config_log_by_build_type BUILD_TYPE)
    # Map log level names to bit positions
    set(FATAL_BIT   0)  # Bit 0: 1 << 0 = 0x1
    set(ERROR_BIT   1)  # Bit 1: 1 << 1 = 0x2
    set(WARNING_BIT 2)  # Bit 2: 1 << 2 = 0x4
    set(INFO_BIT    3)  # Bit 3: 1 << 3 = 0x8
    set(DEBUG_BIT   4)  # Bit 4: 1 << 4 = 0x10
    set(TRACE_BIT   5)  # Bit 5: 1 << 5 = 0x20

    # Start with 0
    set(MASK 0)

    # Process each log level argument
    foreach(LEVEL ${ARGN})
        if(LEVEL STREQUAL "FATAL")
            math(EXPR BIT_VALUE "1 << ${FATAL_BIT}")
        elseif(LEVEL STREQUAL "ERROR")
            math(EXPR BIT_VALUE "1 << ${ERROR_BIT}")
        elseif(LEVEL STREQUAL "WARNING")
            math(EXPR BIT_VALUE "1 << ${WARNING_BIT}")
        elseif(LEVEL STREQUAL "INFO")
            math(EXPR BIT_VALUE "1 << ${INFO_BIT}")
        elseif(LEVEL STREQUAL "DEBUG")
            math(EXPR BIT_VALUE "1 << ${DEBUG_BIT}")
        elseif(LEVEL STREQUAL "TRACE")
            math(EXPR BIT_VALUE "1 << ${TRACE_BIT}")
        else()
            message(WARNING "[skl_config_log_by_build_type] Unknown log level: ${LEVEL}")
            continue()
        endif()

        # OR the bit into the mask
        math(EXPR MASK "${MASK} | ${BIT_VALUE}")
    endforeach()

    # Convert to hex string
    math(EXPR MASK_HEX "${MASK}" OUTPUT_FORMAT HEXADECIMAL)

    # Set the appropriate cache variable
    if(BUILD_TYPE STREQUAL "DEV")
        set(SKL_LOG_LEVEL_DEV "${MASK_HEX}" CACHE STRING "[CORE] Log level mask for DEV build" FORCE)
        message(STATUS "[Skylake] Log level for DEV set to: ${MASK_HEX} (${ARGN})")
    elseif(BUILD_TYPE STREQUAL "STAGING")
        set(SKL_LOG_LEVEL_STAGING "${MASK_HEX}" CACHE STRING "[CORE] Log level mask for STAGING build" FORCE)
        message(STATUS "[Skylake] Log level for STAGING set to: ${MASK_HEX} (${ARGN})")
    elseif(BUILD_TYPE STREQUAL "SHIPPING")
        set(SKL_LOG_LEVEL_SHIPPING "${MASK_HEX}" CACHE STRING "[CORE] Log level mask for SHIPPING build" FORCE)
        message(STATUS "[Skylake] Log level for SHIPPING set to: ${MASK_HEX} (${ARGN})")
    else()
        message(FATAL_ERROR "[skl_config_log_by_build_type] Unknown build type: ${BUILD_TYPE}. Must be DEV, STAGING, or SHIPPING.")
    endif()
endfunction()
