include_guard()

# Create the core skylake library target
function( skl_CreateSkylakeCoreLibTarget 
          TARGET_NAME
          PRESET_NAME )

    # Add the static lib target
    add_library(${TARGET_NAME} STATIC 
        ${SKL_CORE_LIB_SOURCE_FILES} 
        ${SKL_CORE_LIB_PUBLIC_FILES}
        "${SKL_MIMALLOC_DIR}/src/static.cpp")

    # Set include directories
    target_include_directories(${TARGET_NAME} PUBLIC "${SKL_CORE_LIB_SRC_ROOT}/include")
    target_include_directories(${TARGET_NAME} PUBLIC "${SKL_MIMALLOC_DIR}/include")
    target_include_directories(${TARGET_NAME} PUBLIC "${SKL_MAGICENUM_DIR}/include")
    target_include_directories(${TARGET_NAME} PRIVATE "${SKL_CORE_LIB_SRC_ROOT}/source")
    
    # Link libraries
    target_link_libraries(${TARGET_NAME} PUBLIC fmt)

    # Add definitions
    skl_ApplyListItemsAsTargetDefinitions(${TARGET_NAME} PUBLIC SKL_DEFINITIONS_LIST)

    if(SKL_ENABLE_LOG_COLORS)
        target_compile_definitions(${TARGET_NAME} PUBLIC "SKL_ENABLE_LOG_COLORS=1")
    endif()

    # Add compilation options
    skl_ApplyListItemsAsTargetCompileOptions(${TARGET_NAME} PUBLIC SKL_COMPILE_FLAGS_LIST)
    
    # Add sanitization compilation options
    skl_ApplyListItemsAsTargetCompileOptions(${TARGET_NAME} PUBLIC SKL_SANITIZATION_LIST)

    # Add link options
    skl_ApplyListItemsAsTargetLinkOptions(${TARGET_NAME} PUBLIC SKL_LINK_FLAGS_LIST)

    # Add disabled warnings
    skl_ApplyListItemsAsTargetCompileOptions(${TARGET_NAME} PUBLIC SKL_GENERAL_DISABLED_WARNINGS)

    # Remove default prefix
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")

    # Tuning
    skl_add_tune_header_to_target(${TARGET_NAME} "SkylakeCoreLib" "${PRESET_NAME}" "libskl-core")

    # Code coverage
    if(SKL_ENABLE_CODE_COVERAGE)
        skl_EnableCodeCoverageForTarget(${TARGET_NAME})
    endif()

    # Enable unity build
    set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD ON)
    set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD_BATCH_SIZE 24)
endfunction()

# Create skl lib test targets
function( skl_CreateSkylakeLibTestTarget 
          TARGET_NAME
          DEP_LIB_TARGET_NAME,
          optional_LIST_OF_PRIVATE_HEADERS
          optional_PRIVATE_HEADERS_DIR )

    # Create the test executable target
    add_executable(${TEST_TARGET_NAME} ${SKL_LIB_TESTS_HEDAER_FILES} ${TEST_SOURCE_FILE})

    # Link dependencies
    target_link_libraries(${TEST_TARGET_NAME} PUBLIC "gtest_main")
    target_link_libraries(${TEST_TARGET_NAME} PUBLIC "gmock")
    
    foreach(LIB IN LISTS DEP_LIB_TARGET_NAME)
        if(TARGET ${LIB})
            target_link_libraries(${TEST_TARGET_NAME} PUBLIC "${LIB}")
        endif()
    endforeach()

    # Set include directories
    target_include_directories(${TEST_TARGET_NAME} PRIVATE "${SKL_LIB_TESTS_ROOT_DIR}/private")

    # Special defines
    target_compile_definitions(${TEST_TARGET_NAME} PRIVATE "SKL_TEST=1")

    # Add test
    add_test(NAME ${TEST_TARGET_NAME}_Test COMMAND ${TEST_TARGET_NAME})

    # Set output dir
    set_target_properties(${TEST_TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/all_skl_tests/"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/all_skl_tests/"
    )
    
    # Add list of private headers
    set(TEMP_LIST "${optional_LIST_OF_PRIVATE_HEADERS}")
    target_sources(${TEST_TARGET_NAME} PUBLIC ${TEMP_LIST})

    # Add private headers inlcude dir
    set(TEMP_LIST "${optional_PRIVATE_HEADERS_DIR}")
    target_include_directories(${TEST_TARGET_NAME} PUBLIC ${TEMP_LIST})

    # Make dependency of skl_tests
    add_dependencies(skl_tests ${TEST_TARGET_NAME})

endfunction()

# Create one skl lib test target for each given source file
function( skl_CreateSkylakeLibTestTargetsFromSourceFiles
          SOURCE_FILES 
          TARGET_NAME_PREFIX
          DEP_LIB_TARGET_NAME
          optional_LIST_OF_PRIVATE_HEADERS
          optional_PRIVATE_HEADERS_DIR )

    foreach(TEST_SOURCE_FILE ${SOURCE_FILES})
        # Prepare the test target name
        get_filename_component(TEST_SOURCE_FILE_NAME_WE ${TEST_SOURCE_FILE} NAME_WE)
        set(TEST_TARGET_NAME "${TARGET_NAME_PREFIX}_${TEST_SOURCE_FILE_NAME_WE}")
    
        # Create the target
        skl_CreateSkylakeLibTestTarget(${TEST_TARGET_NAME} "${DEP_LIB_TARGET_NAME}" "${optional_LIST_OF_PRIVATE_HEADERS}" "${optional_PRIVATE_HEADERS_DIR}")
    endforeach()

endfunction()

# Create the dev targets
function( skl_CreateDevTargets )

    skl_CreateSkylakeCoreLibTarget("libskl-core-dev" "")
    
    if(SKL_ENABLE_DEV_TEST_TARGETS)
        add_custom_target(skl_tests)

        skl_CreateSkylakeLibTestTargetsFromSourceFiles("${SKL_CORE_LIB_TESTS_SOURCE_FILES}" "skl_core" "libskl-core-dev" "" "")
    endif()

endfunction()
