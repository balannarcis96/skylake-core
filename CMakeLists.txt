#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
cmake_minimum_required(VERSION 4.0.0)
project(SkylakeCore LANGUAGES CXX VERSION 1.0.0)

# C++ 23
set(CMAKE_CXX_STANDARD 23 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "" FORCE)

# No c++ modules
set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "" FORCE)

# Clang STD lib
add_compile_options(-stdlib=libc++)
add_link_options(-lc++ -lc++abi)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)

# Update modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tuning")

include(SkylakeTuning)
include(SkylakeCoreOptions)
include(SkylakeValidatePlatform)

if(SKL_CORE_ENABLE_TESTS)
    include(SkylakeCoreTest)
endif()

# Skylake Core Lib
set(SKL_CORE_LIB_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE STRING "libskl-core root dir" FORCE)
file(GLOB_RECURSE _SKL_CORE_SOURCE_FILES "${SKL_CORE_LIB_SRC_ROOT}/source/*.cpp")
file(GLOB_RECURSE _SKL_CORE_PUBLIC_FILES "${SKL_CORE_LIB_SRC_ROOT}/include/*/*")

set(SKL_CORE_LIB_SOURCE_FILES "${_SKL_CORE_SOURCE_FILES}" CACHE STRING "libskl-core source files" FORCE)
set(SKL_CORE_LIB_PUBLIC_FILES "${_SKL_CORE_PUBLIC_FILES}" CACHE STRING "libskl-core public include files" FORCE)

# Third party
add_subdirectory(third_party)

# Create the core skylake library target
function(skl_CreateSkylakeCoreLibTarget
    TARGET_NAME
    PRESET_NAME)

    # ENABLE_RTTI  [Optional] Enable RTTI (default=OFF)
    # DISABLE_LTO  [Optional] Do not not LTO (link time optimization) in shipping builds (default=OFF)
    # FORCE_HUGEPAGE_SUPPORT [Optional] Force huge page support (default=OFF)
    # NO_MIMALLOC [Optional] Disable mimalloc (default=OFF)
    # OVERRIDE_LOG_MASK [Optional] Override the log level mask (hex value like 0x3F)

    set(_OPTIONS ENABLE_RTTI DISABLE_LTO FORCE_HUGEPAGE_SUPPORT NO_MIMALLOC)
    set(_SINGLE_VALUE_ARGS OVERRIDE_LOG_MASK)
    cmake_parse_arguments(SKL "${_OPTIONS}" "${_SINGLE_VALUE_ARGS}" "" ${ARGN})

    # Add the static lib target
    add_library(${TARGET_NAME} STATIC
        ${SKL_CORE_LIB_SOURCE_FILES}
        ${SKL_CORE_LIB_PUBLIC_FILES})

    # Add mimalloc if not disabled
    if(NOT SKL_NO_MIMALLOC)
        # Use mimalloc as the default allocator 
        target_compile_definitions(${TARGET_NAME} PUBLIC SKL_CORE_USE_MIMALLOC=1)

        # Add mimalloc source files
        target_sources(${TARGET_NAME} PRIVATE
            "${SKL_MIMALLOC_DIR}/src/static.c"
        )

        # Compile mimalloc with the C++ compiler
        set_source_files_properties("${SKL_MIMALLOC_DIR}/src/static.c" PROPERTIES
            LANGUAGE CXX
        )

        # Mimalloc has an unused function when built in Release mode
        set_source_files_properties("${SKL_MIMALLOC_DIR}/src/static.c" PROPERTIES
            COMPILE_OPTIONS "-Wno-unused-function"
        )

        # Set include directories
        target_include_directories(${TARGET_NAME} PUBLIC "${SKL_MIMALLOC_DIR}/include")
    endif()

    # Set include directories
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC "${SKL_CORE_LIB_SRC_ROOT}/include")
    target_include_directories(${TARGET_NAME} PUBLIC "${SKL_MAGICENUM_DIR}/include")
    target_include_directories(${TARGET_NAME} PRIVATE "${SKL_CORE_LIB_SRC_ROOT}/source")

    # Link libraries
    target_link_libraries(${TARGET_NAME} PUBLIC fmt)

    # Definitions
    target_compile_definitions(${TARGET_NAME} PUBLIC
        "SKL_CACHE_LINE_SIZE=64"
        "SKL_CACHE_ALIGNED=alignas(64)"
        "SKL_NOINLINE=[[clang::noinline]]"
    )

    # Add compilation options
    target_compile_options(${TARGET_NAME} PUBLIC
        -Wall
        -Werror
        -gdwarf-4
        -pthread
        -fvisibility=hidden)

    # Add link options
    target_link_options(${TARGET_NAME} PUBLIC -pthread)

    if(SKL_CORE_NO_EXCEPTIONS) # No exceptions
        target_compile_options(${TARGET_NAME} PUBLIC -fno-exceptions)
        target_compile_definitions(${TARGET_NAME} PUBLIC SKL_NO_EXCEPTIONS=1)
    endif()

    # Determine log level mask (allow override)
    if(SKL_OVERRIDE_LOG_MASK)
        set(_LOG_LEVEL_MASK "${SKL_OVERRIDE_LOG_MASK}")
    elseif("${SKL_BUILD_TYPE}" STREQUAL "DEV")
        set(_LOG_LEVEL_MASK "${SKL_LOG_LEVEL_DEV}")
    elseif("${SKL_BUILD_TYPE}" STREQUAL "STAGING")
        set(_LOG_LEVEL_MASK "${SKL_LOG_LEVEL_STAGING}")
    elseif("${SKL_BUILD_TYPE}" STREQUAL "SHIPPING")
        set(_LOG_LEVEL_MASK "${SKL_LOG_LEVEL_SHIPPING}")
    endif()

    if("${SKL_BUILD_TYPE}" STREQUAL "DEV")
        target_compile_definitions(${TARGET_NAME} PUBLIC
            "SKL_BUILD_DEV=1"
            "SKL_LOG_LEVEL_MASK=${_LOG_LEVEL_MASK}")
    elseif("${SKL_BUILD_TYPE}" STREQUAL "STAGING")
        target_compile_definitions(${TARGET_NAME} PUBLIC
            "SKL_BUILD_STAGING=1"
            "SKL_LOG_LEVEL_MASK=${_LOG_LEVEL_MASK}")
    elseif("${SKL_BUILD_TYPE}" STREQUAL "SHIPPING")
        target_compile_options(${TARGET_NAME} PUBLIC
            -finline-functions # Aggressive inlining
            -fno-stack-protector # [Disabled] Detects some buffer overruns that overwrite a function's return address
            -ffast-math # Floating point - fast

            -march=native # Build for the current platform
            -mtune=native # Fine tune scheduling
            -fomit-frame-pointer # Smaller fn prologues
            -fdata-sections # Put each symbol in its own section
            -ffunction-sections # Put each symbol in its own section
        )
        target_link_options(${TARGET_NAME} PUBLIC
            -Wl,--gc-sections # Strip dead code produced by the 2-section split
        )
        target_compile_definitions(${TARGET_NAME} PUBLIC
            "SKL_BUILD_SHIPPING=1"
            "SKL_LOG_LEVEL_MASK=${_LOG_LEVEL_MASK}")

        if(NOT SKL_ENABLE_RTTI)
            target_compile_options(${TARGET_NAME} PUBLIC -fno-rtti) # Disable RTTI
        endif()

        if(NOT SKL_DISABLE_LTO)
            target_compile_options(${TARGET_NAME} PUBLIC -flto=full) # Link time optimizations
            target_link_options(${TARGET_NAME} PUBLIC -flto=full)    # Link time optimizations
        endif()

        # FMT workaround: https://github.com/surge-synthesizer/surge/issues/7730
        target_compile_options(${TARGET_NAME} PUBLIC -Wno-nan-infinity-disabled)
    endif()

    # Enable sanitizers if requested and possible
    if(("${SKL_BUILD_TYPE}" STREQUAL "DEV") OR ("${SKL_BUILD_TYPE}" STREQUAL "STAGING"))
        if(SKL_CORE_ENABLE_SANITIZATION)
            target_compile_options(${TARGET_NAME} PUBLIC
                -fsanitize=address # Address sanitization
            )
            target_link_options(${TARGET_NAME} PUBLIC
                -fsanitize=address # Address sanitization
            )
        endif()
    endif()

    if(SKL_CORE_ENABLE_TESTS)
        target_compile_definitions(${TARGET_NAME} PUBLIC "SKL_CORE_TESTING=1")
    endif()

    # Enable huge page support if requested
    if(SKL_CORE_FORCE_HUGEPAGE_SUPPORT)
        target_compile_definitions(${TARGET_NAME} PUBLIC "SKL_CORE_FORCE_HUGEPAGE_SUPPORT=1")
    endif()

    # Add disabled warnings
    skl_ApplyListItemsAsTargetCompileOptions(${TARGET_NAME} PUBLIC SKL_GENERAL_DISABLED_WARNINGS)

    # Remove default prefix
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")

    # Tuning
    skl_add_tune_header_to_target(${TARGET_NAME} "skl_core" PRESET_NAME "${PRESET_NAME}" PRESET_TARGET_NAME "libskl-core")

    # Code coverage
    if(SKL_ENABLE_CODE_COVERAGE)
        skl_EnableCodeCoverageForTarget(${TARGET_NAME})
    endif()

    if("${SKL_BUILD_TYPE}" STREQUAL "SHIPPING")
        # Enable unity build
        set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD ON)
        set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD_BATCH_SIZE 24)

        # Exclude buffer pool files from unity builds to avoid symbol conflicts
        set_source_files_properties(
            "${SKL_CORE_LIB_SRC_ROOT}/source/buffer_pool.cpp"
            "${SKL_CORE_LIB_SRC_ROOT}/source/hugepage_buffer_pool.cpp"
            PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
        )
    endif()
endfunction()

if(PROJECT_IS_TOP_LEVEL)
    # Enable clangd
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Add presets
    skl_add_presets_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/presets/dev_presets.json")
    skl_add_presets_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/presets/qa_presets.json")

    # Create development targets
    skl_CreateSkylakeCoreLibTarget("libskl-core-dev" "")

    # Add tests if possible
    if(SKL_CORE_ENABLE_TESTS)
        # Enable CTest
        enable_testing()

        # Add the gtest library
        if(NOT TARGET gtest)
            set(BUILD_GMOCK OFF)
            set(INSTALL_GTEST OFF)
            add_subdirectory(third_party/googletest)
        endif()

        # Add tests
        add_subdirectory(test)
    endif()
else()
    if(SKL_CORE_ADD_PRESETS)
        # Add presets
        skl_add_presets_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/presets/dev_presets.json")
        skl_add_presets_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/presets/qa_presets.json")
    endif()

    if("${SKL_BUILD_TYPE}" STREQUAL "SHIPPING")
        set(SKL_IS_SHIPPING ON CACHE BOOL "[Skylake] Is Shipping" FORCE)
    else()
        set(SKL_IS_SHIPPING OFF CACHE BOOL "[Skylake] Is Shipping" FORCE)
    endif()
endif()
