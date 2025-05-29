#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

function( skl_AddCoreTest
          DIRECTORY )

    if(NOT TARGET gtest)
        message(FATAL_ERROR "Tests require the gtest library!")
    endif()

    get_filename_component(_DIRECTORY_NAME "${DIRECTORY}" NAME)
    set(_TARGET_NAME "skl-core-test-${_DIRECTORY_NAME}")
    file(GLOB _SOURCE_FILES "${DIRECTORY}/*.cpp")

    add_executable(
        ${_TARGET_NAME}
        ${_SOURCE_FILES}
    )

    target_include_directories(${_TARGET_NAME} PUBLIC ${DIRECTORY})

    # Link skylake core
    target_link_libraries(${_TARGET_NAME} PUBLIC "libskl-core-dev")

    # Link gtest
    target_link_libraries(${_TARGET_NAME} PUBLIC "gtest_main")

    # Link gmock
    if(TARGET gmock)
        target_link_libraries(${_TARGET_NAME} PUBLIC "gmock")
    endif()

    set_target_properties(${_TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/skl-core-tests")

    # CTest
    add_test(
        NAME ${_TARGET_NAME}
        COMMAND ${_TARGET_NAME}
    )

endfunction()

# Note: Requires the test to define the main() function
function( skl_AddStandaloneCoreTest
          DIRECTORY )

    if(NOT TARGET gtest)
        message(FATAL_ERROR "Tests require the gtest library!")
    endif()

    get_filename_component(_DIRECTORY_NAME "${DIRECTORY}" NAME)
    set(_TARGET_NAME "skl-core-test-${_DIRECTORY_NAME}")
    file(GLOB _SOURCE_FILES "${DIRECTORY}/*.cpp")
    
    add_executable(
        ${_TARGET_NAME}
        ${_SOURCE_FILES}
    )
    
    target_include_directories(${_TARGET_NAME} PUBLIC ${DIRECTORY})

    # Link skylake core
    target_link_libraries(${_TARGET_NAME} PUBLIC "libskl-core-dev")
    
    # Link gtest
    target_link_libraries(${_TARGET_NAME} PUBLIC "gtest")

    # Link gmock
    if(TARGET gmock)
        target_link_libraries(${_TARGET_NAME} PUBLIC "gmock")
    endif()
    
    set_target_properties(${_TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/skl-core-tests")

    # CTest
    add_test(
        NAME ${_TARGET_NAME}
        COMMAND ${_TARGET_NAME}
    )

endfunction()
