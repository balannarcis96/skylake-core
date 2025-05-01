#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

function( skl_StringContains 
          MAIN_STRING 
          SUB_STRING 
          RESULT_VAR )

    string(FIND "${MAIN_STRING}" "${SUB_STRING}" SUB_STRING_INDEX)
    if(SUB_STRING_INDEX GREATER_EQUAL 0)
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
    endif()

endfunction()

macro ( skl_AppendToCachedList 
        LIST_VAR
        STRING_TO_APPEND )

    if(${LIST_VAR} STREQUAL "")
        set(${LIST_VAR} "${STRING_TO_APPEND}" CACHE STRING "" FORCE)
    else()
        set(${LIST_VAR} "${${LIST_VAR}};${STRING_TO_APPEND}" CACHE STRING "" FORCE)
    endif()

endmacro()

function( skl_ApplyListItemsAsTargetDefinitions
          TARGET_NAME
          SCOPE
          LIST_OF_DEFS )

    foreach(RAW_DEF IN LISTS ${LIST_OF_DEFS})
        set(DEF "-D${RAW_DEF}")
        target_compile_definitions(${TARGET_NAME} ${SCOPE} ${DEF})
    endforeach()

endfunction()

function( skl_ApplyListItemsAsTargetCompileOptions
          TARGET_NAME
          SCOPE
          LIST_OF_OPTIONS )

    foreach(OPT IN LISTS ${LIST_OF_OPTIONS})
        target_compile_options(${TARGET_NAME} ${SCOPE} ${OPT})
    endforeach()

endfunction()

function( skl_ApplyListItemsAsTargetLinkOptions
          TARGET_NAME
          SCOPE
          LIST_OF_OPTIONS )

    foreach(OPT IN LISTS ${LIST_OF_OPTIONS})
        target_link_options(${TARGET_NAME} ${SCOPE} ${OPT})
    endforeach()

endfunction()

function( skl_PrintAllItemsInList
          LIST_OF_ITEMS )

    foreach(ITEM IN LISTS ${LIST_OF_ITEMS})
        message("${ITEM}")
    endforeach()

endfunction()

function( skl_AssertValid2ByteHexNumber 
          INPUT_STRING )

    if (NOT "${INPUT_STRING}" MATCHES "^0x[0-9A-Fa-f]+$")
        message(FATAL_ERROR "Input string '${INPUT_STRING}' is not a valid hexadecimal number.")
    endif()

endfunction()

function( skl_EnableCodeCoverageForTarget
          TARGET_NAME )

    target_compile_options(${TARGET_NAME} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_libraries(${TARGET_NAME} PRIVATE -fprofile-instr-generate -fcoverage-mapping)

endfunction()

function( skl_PropagateDefinitions 
            SOURCE_TARGET 
            DEST_TARGET )

    get_target_property(definitions ${SOURCE_TARGET} COMPILE_DEFINITIONS)
    
    if(definitions)
        target_compile_definitions(${DEST_TARGET} PRIVATE ${definitions})
    endif()

endfunction()

function( skl_PrintAllIncludeDirs
            SOURCE_TARGET )

    get_property(INCLUDE_DIRS TARGET ${SOURCE_TARGET} PROPERTY INCLUDE_DIRECTORIES)
    message(STATUS "Include directories for ${SOURCE_TARGET}: ${INCLUDE_DIRS}")
        
endfunction()
