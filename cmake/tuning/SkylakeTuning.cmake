#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

set(SKL_TUNE_PRESETS_FILES "" CACHE STRING "Tuning presets json file list" FORCE)
set(SKL_TUNE_PRESET_PROCESS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/generate_tuning.py" CACHE STRING "Tuning preset processing python script")
set(SKL_TUNE_GENERATE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/tuning/" CACHE STRING "Generated tuing files output path")
set(SKL_TUNE_DEFAULT_PRESETS_FILE "${CMAKE_CURRENT_LIST_DIR}/../presets/default_presets.json" CACHE STRING "The default presets file")
set(SKL_TUNE_DEFAULT_PRESET "default" CACHE STRING "Default tuning preset to use (as base of all presets)")
set(SKL_TUNE_PRESET "default" CACHE STRING "Current preset name to use")
set(SKL_TUNE_VERBOSE OFF CACHE BOOL "Verbose tuning files generation")

if(NOT SKL_TUNE_DEFAULT_PRESET)
    message(FATAL_ERROR "Please provide the default preset name to use from the presets files! [eg. -DSKL_TUNE_DEFAULT_PRESET=PRESET_NAME]")
endif()

if(NOT EXISTS ${SKL_TUNE_PRESET_PROCESS_SCRIPT})
    message(FATAL_ERROR "Invalid presets parsing python script (SKL_TUNE_PRESET_PROCESS_SCRIPT=${SKL_TUNE_PRESET_PROCESS_SCRIPT})!")
endif()

# Create the output dir if necessary
if(NOT EXISTS ${SKL_TUNE_GENERATE_OUTPUT_PATH})
    file(MAKE_DIRECTORY ${SKL_TUNE_GENERATE_OUTPUT_PATH})
endif()

function(skl_add_presets_file
    FILE_NAME)
    if(NOT EXISTS ${FILE_NAME})
        message(FATAL_ERROR "Preset file doesnt exists ${FILE_NAME}!")
    endif()

    message(STATUS "Using presets file ${FILE_NAME}")

    if("${SKL_TUNE_PRESETS_FILES}" STREQUAL "")
        set(SKL_TUNE_PRESETS_FILES "${FILE_NAME}" CACHE STRING "Tuning presets json file list" FORCE)
    else()
        set(SKL_TUNE_PRESETS_FILES "${SKL_TUNE_PRESETS_FILES};${FILE_NAME}" CACHE STRING "Tuning presets json file list" FORCE)
    endif()
endfunction()

function(skl_add_tune_header_to_target
    TARGET_NAME # [Required] Name of the target to add tune headers to
    OUTPUT_FILE_NAME) # [Required] Base of the output tune files (private and public)
    # PRESET_NAME         [Optional] Name of the preset to use | if not provided SKL_TUNE_PRESET is used
    # PRESET_TARGET_NAME  [Optional] Used when multiple targets must use the same preset target name | default = TARGET_NAME
    # DEFAULT_PRESET_FILE [Optional] Default presets file name | if the default preset file for the target is not ${SKL_TUNE_DEFAULT_PRESETS_FILE}
    # DEFAULT_PRESET_NAME [Optional] Default preset name |  if the default preset name for the target is not ${SKL_TUNE_DEFAULT_PRESET}
    set(_ONE_VALUE_ARGS PRESET_NAME PRESET_TARGET_NAME DEFAULT_PRESET_FILE DEFAULT_PRESET_NAME)
    cmake_parse_arguments(TUNE "" "${_ONE_VALUE_ARGS}" "" ${ARGN})

    if(NOT TUNE_PRESET_NAME)
        set(TUNE_PRESET_NAME "${SKL_TUNE_PRESET}")
    endif()

    # Used so multiple targets use the same target name in the preset
    if(NOT TUNE_PRESET_TARGET_NAME)
        set(TUNE_PRESET_TARGET_NAME "${TARGET_NAME}")
    endif()

    if(NOT TUNE_DEFAULT_PRESET_FILE)
        set(TUNE_DEFAULT_PRESET_FILE "${SKL_TUNE_DEFAULT_PRESETS_FILE}")
    endif()

    if(NOT TUNE_DEFAULT_PRESET_NAME)
        set(TUNE_DEFAULT_PRESET_NAME "${SKL_TUNE_DEFAULT_PRESET}")
    endif()

    message(STATUS "Adding tuning header to target ${TARGET_NAME} with preset ${TUNE_PRESET_NAME}")

    get_target_property(BINARY_DIR ${TARGET_NAME} RUNTIME_OUTPUT_DIRECTORY)

    set(TARGET_PRIVATE_TUNE_FILE_NAME "tune_${OUTPUT_FILE_NAME}_private.h")
    set(TARGET_PRIVATE_TUNE_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/tmp/${TARGET_PRIVATE_TUNE_FILE_NAME}")
    set(TARGET_PRIVATE_TUNE_FINAL_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/${TARGET_PRIVATE_TUNE_FILE_NAME}")

    set(TARGET_PUBLIC_TUNE_FILE_NAME "tune_${OUTPUT_FILE_NAME}_public.h")
    set(TARGET_PUBLIC_TUNE_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/tmp/${TARGET_PUBLIC_TUNE_FILE_NAME}")
    set(TARGET_PUBLIC_TUNE_FINAL_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/${TARGET_PUBLIC_TUNE_FILE_NAME}")

    # Create the target dir if necessary
    if(NOT EXISTS "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}")
        file(MAKE_DIRECTORY "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}")
    endif()

    # Create the tmp dir if necessary
    if(NOT EXISTS "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/tmp")
        file(MAKE_DIRECTORY "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/tmp")
    endif()

    set("VERBOSE_VALUE" "False")

    if(SKL_TUNE_VERBOSE)
        set("VERBOSE_VALUE" "True")
    endif()

    execute_process(
        COMMAND
        python3 "${SKL_TUNE_PRESET_PROCESS_SCRIPT}"
        --preset_name "${TUNE_PRESET_NAME}"
        --input_file "${SKL_TUNE_PRESETS_FILES}"
        --target_name "${TARGET_NAME}"
        --preset_target_name "${TUNE_PRESET_TARGET_NAME}"
        --output_public "${TARGET_PUBLIC_TUNE_FILE}"
        --output_private "${TARGET_PRIVATE_TUNE_FILE}"
        --default_file "${TUNE_DEFAULT_PRESET_FILE}"
        --default_preset "${TUNE_DEFAULT_PRESET_NAME}"
        --verbose "${VERBOSE_VALUE}"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Configure the private tune file
    configure_file(
        ${TARGET_PRIVATE_TUNE_FILE}
        ${TARGET_PRIVATE_TUNE_FINAL_FILE}
    )

    # Configure the private tune file
    configure_file(
        ${TARGET_PUBLIC_TUNE_FILE}
        ${TARGET_PUBLIC_TUNE_FINAL_FILE}
    )

    # Clean temp
    file(REMOVE_RECURSE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}/tmp/")

    # Add the header to the public include set of the target
    target_include_directories(${TARGET_NAME} PUBLIC "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${TARGET_NAME}")
endfunction()
