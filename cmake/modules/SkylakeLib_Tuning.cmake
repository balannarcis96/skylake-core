include_guard()

set(SKL_TUNE_PRESETS_FILES "" CACHE STRING "Tuning presets json file list" FORCE)
set(SKL_TUNE_PRESET_PROCESS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../py/generate_tuning.py" CACHE STRING "Tuning preset processing python script")
set(SKL_TUNE_GENERATE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/tuning/" CACHE STRING "Generated tuing files output path")
set(SKL_TUNE_DEFAULT_PRESETS_FILE "${CMAKE_CURRENT_LIST_DIR}/../presets/default_presets.json" CACHE STRING "The default presets file")
set(SKL_TUNE_DEFAULT_PRESET "default" CACHE STRING "Default tuning preset to use (as base of all presets)")
set(SKL_TUNE_PRESET "default" CACHE STRING "Current preset name to use")
set(SKL_TUNE_VERBOSE OFF CACHE BOOL "Verbose tuning files generation")

if(NOT SKL_TUNE_DEFAULT_PRESET)
    message(FATAL_ERROR "Please provide the default preset name to use from the presets files! [eg. -DSKL_TUNE_DEFAULT_PRESET=preset_name]")
endif()

if(NOT EXISTS ${SKL_TUNE_PRESET_PROCESS_SCRIPT})
    message(FATAL_ERROR "Invalid presets parsing python script (SKL_TUNE_PRESET_PROCESS_SCRIPT=${SKL_TUNE_PRESET_PROCESS_SCRIPT})!")
endif()

# Create the output dir if necessary
if(NOT EXISTS ${SKL_TUNE_GENERATE_OUTPUT_PATH})
    file(MAKE_DIRECTORY ${SKL_TUNE_GENERATE_OUTPUT_PATH})
endif()

function( skl_add_presets_file 
        file_name )
    if(NOT EXISTS ${file_name})
        message(FATAL_ERROR "Preset file doesnt exists ${file_name}!")
    endif()
    message(STATUS "Using presets file ${file_name}")
    if("${SKL_TUNE_PRESETS_FILES}" STREQUAL "")
        set(SKL_TUNE_PRESETS_FILES "${file_name}" CACHE STRING "Tuning presets json file list" FORCE)
    else()
        set(SKL_TUNE_PRESETS_FILES "${SKL_TUNE_PRESETS_FILES};${file_name}" CACHE STRING "Tuning presets json file list" FORCE)
    endif()
endfunction()

function( skl_add_tune_header_to_target 
    target_name          # [Required] Name of the target to add tune headers to
    output_file_name     # [Required] Base of the output tune files (private and public)
    preset_name          # [Optional] Name of the preset to use | if not provided SKL_TUNE_PRESET is used
    preset_target_name ) # [Optional] Used when multiple targets must use the same preset target name | default = target_name
    
    if(NOT preset_name)
        set(preset_name "${SKL_TUNE_PRESET}")
    endif()

    # Used so multiple targets use the same target name in the preset
    if(NOT preset_target_name)
        set(preset_target_name "${target_name}")
    endif()

    message(STATUS "Adding tuning header to target ${target_name} with preset ${preset_name}")

    get_target_property(BINARY_DIR ${target_name} RUNTIME_OUTPUT_DIRECTORY)

    set(TARGET_PRIVATE_TUNE_FILE_NAME "Tune_${output_file_name}_private.h")
    set(TARGET_PRIVATE_TUNE_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/tmp/${TARGET_PRIVATE_TUNE_FILE_NAME}")
    set(TARGET_PRIVATE_TUNE_FINAL_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/${TARGET_PRIVATE_TUNE_FILE_NAME}")

    set(TARGET_PUBLIC_TUNE_FILE_NAME "Tune_${output_file_name}_public.h")
    set(TARGET_PUBLIC_TUNE_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/tmp/${TARGET_PUBLIC_TUNE_FILE_NAME}")
    set(TARGET_PUBLIC_TUNE_FINAL_FILE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/${TARGET_PUBLIC_TUNE_FILE_NAME}")

    # Create the target dir if necessary
    if(NOT EXISTS "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}")
        file(MAKE_DIRECTORY "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}")
    endif()

    # Create the tmp dir if necessary
    if(NOT EXISTS "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/tmp")
        file(MAKE_DIRECTORY "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/tmp")
    endif()

    set("VERBOSE_VALUE" "False")
    if(SKL_TUNE_VERBOSE)
        set("VERBOSE_VALUE" "True")
    endif()

    # Run the tuning generator script
    execute_process(
        COMMAND python3 ${SKL_TUNE_PRESET_PROCESS_SCRIPT} --preset_name "${preset_name}" --input_file "${SKL_TUNE_PRESETS_FILES}" --target_name "${target_name}" --preset_target_name "${preset_target_name}" --output_public "${TARGET_PUBLIC_TUNE_FILE}" --output_private "${TARGET_PRIVATE_TUNE_FILE}" --default_file "${SKL_TUNE_DEFAULT_PRESETS_FILE}" --default_preset ${SKL_TUNE_DEFAULT_PRESET} --verbose ${VERBOSE_VALUE}
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
    file(REMOVE_RECURSE "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}/tmp/")

    # Add the header to the public include set of the target
    target_include_directories(${target_name} PUBLIC "${SKL_TUNE_GENERATE_OUTPUT_PATH}/${target_name}")
endfunction()
