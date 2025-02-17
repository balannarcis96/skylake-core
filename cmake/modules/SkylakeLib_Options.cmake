include_guard()

include(SkylakeLib_OptionsValues)
include(SkylakeLib_Utils)

# Descriptions
set(SKL_CACHE_LINE_MEM_MANAGER_DESC "Allign all memory blocks inside the MemoryManager to the cache line, removing any possiblity of false sharing between memory blocks in the MemoryManager at the cost of more memory consumtion" )
set(SKL_USE_PRECISE_SLEEP_DESC "Use very precise sleep function for the active, non-task-handling workers")
set(SKL_ENABLE_LIKELY_FLAGS_DESC "SkylakeLib is laced out with SKL_LIKELY, enable SKL_LIKELY?")
set(SKL_ENABLE_UNLIKELY_FLAGS_DESC "SkylakeLib is laced out with SKL_UNLIKELY, enable SKL_UNLIKELY?")

# Build type
set(SKL_BUILD_TYPE "DEV" CACHE STRING "[ROOT] Build type")

# Root options
option(SKL_ENABLE_TESTS             "[ROOT] Include tests srource"                      ON)
option(SKL_ENABLE_DEV_TEST_TARGETS  "[ROOT] Add the dev test targets"                   ON)
option(SKL_DEV                      "[ROOT] Add dev targets"                            ON)
option(SKL_ADD_GTEST_GMOCK          "[ROOT] Add the internal gtest and gmock targets"   ON)
  
# Misc  
option(SKL_NO_ASSERTS               "[MISC] Disabled all runtime asserts"              OFF)
option(SKL_ENABLE_SANITIZER_ADDRESS "[MISC] Enable memory sanitizer"                    ON)
option(SKL_ENABLE_SANITIZER_THREAD  "[MISC] Enable thread sanitizer"                   OFF)
option(SKL_ENABLE_SANITIZER_LEAK    "[MISC] Enable leak sanitizer"                     OFF)
option(SKL_ENABLE_SANITIZER_UB      "[MISC] Enable Undefined Behavior sanitizer"       OFF)
option(SKL_ENABLE_SANITIZER_MISC    "[MISC] Enable other misc. sanitizations"          OFF)
option(SKL_USE_PRECISE_SLEEP        "[MISC] ${SKL_USE_PRECISE_SLEEP_DESC}"              ON)
option(SKL_ENABLE_LIKELY_FLAGS      "[MISC] ${SKL_ENABLE_LIKELY_FLAGS_DESC}"           OFF)
option(SKL_ENABLE_UNLIKELY_FLAGS    "[MISC] ${SKL_ENABLE_UNLIKELY_FLAGS_DESC}"         OFF)
option(SKL_WALL                     "[MISC] Enable -Wall (with some sane exceptions)"   ON)
option(SKL_WERROR                   "[MISC] Enable -Werror (with some sane exceptions)" ON)
option(SKL_ENABLE_LOG_COLORS        "[MISC] Allow logging to use ANSI colors"           ON)
option(SKL_ENABLE_CODE_COVERAGE     "[MISC] Generate code coverage artefacts "         OFF)

# Misc properties
set(SKL_LIB_LOG_LEVEL      DEBUG   CACHE STRING "[MISC] SkylakeLib minimum log level to process" FORCE)
set(SKL_REAL_TYPE          Double  CACHE STRING "[MISC] Real type used in the math abstractions(float/double)" FORCE)
set(SKL_L1_CACHE_LINE_SIZE 64bytes CACHE STRING "[MISC] Expected L1 cache line size" FORCE)
set(SKL_LTO                NONE    CACHE STRING "[MISC] LTO variant to use at build" FORCE)

if(SKL_BUILD_TYPE STREQUAL "SHIPPING")
    if((NOT CMAKE_BUILD_TYPE STREQUAL "Release") AND (NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        message(FATAL_ERROR "SHIPPING must be built in RelWithDebInfo or Release!")
    endif()

    set(SKL_LIB_LOG_LEVEL           ERROR CACHE STRING "" FORCE) # ERROR log level
    set(SKL_NO_ASSERTS                                       ON) # No asserts 
    set(SKL_ENABLE_SANITIZER_ADDRESS                        OFF) # No asan
    set(SKL_ENABLE_SANITIZER_THREAD                         OFF) # No tsan
    set(SKL_ENABLE_SANITIZER_LEAK                           OFF) # No lsan
    set(SKL_ENABLE_SANITIZER_UB                             OFF) # No ubsan
    set(SKL_ENABLE_SANITIZER_MISC                           OFF) # No other misc sanitizations
    set(SKL_LTO                      FULL CACHE STRING "" FORCE) # Full LTO
    set(SKL_ENABLE_TESTS                                    OFF) # No tests
    set(SKL_ENABLE_DEV_TEST_TARGETS                         OFF) # No test targets
    set(SKL_ENABLE_CODE_COVERAGE                            OFF) # No code coverage

    message("-- [SKL] SHIPPING BUILD!")
elseif(SKL_BUILD_TYPE STREQUAL "SANITIZING")
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "SANITIZING must be built in Debug!")
    endif()

    set(SKL_LIB_LOG_LEVEL           DEBUG CACHE STRING "" FORCE) # DEBUG log level
    set(SKL_NO_ASSERTS                                      OFF) # Enable asserts 
    set(SKL_ENABLE_SANITIZER_ADDRESS                         ON) # Enable asan
    set(SKL_ENABLE_SANITIZER_THREAD                          ON) # Enable tsan
    set(SKL_ENABLE_SANITIZER_LEAK                            ON) # Enable lsan
    set(SKL_ENABLE_SANITIZER_UB                              ON) # Enable ubsan
    set(SKL_ENABLE_SANITIZER_MISC                            ON) # Enable other misc sanitizations
    set(SKL_LTO                      THIN CACHE STRING "" FORCE) # Thin LTO

    message("-- [SKL] SANITIZING BUILD!")
elseif((SKL_BUILD_TYPE STREQUAL "STAGING") OR (SKL_BUILD_TYPE STREQUAL "STAGE"))
    # All build types are allowed

    set(SKL_LIB_LOG_LEVEL         WARNING CACHE STRING "" FORCE) # WARNING log level
    set(SKL_NO_ASSERTS                                      OFF) # No asserts 
    set(SKL_ENABLE_SANITIZER_ADDRESS                        OFF) # No asan
    set(SKL_ENABLE_SANITIZER_THREAD                         OFF) # No tsan
    set(SKL_ENABLE_SANITIZER_LEAK                           OFF) # No lsan
    set(SKL_ENABLE_SANITIZER_UB                             OFF) # No ubsan
    set(SKL_ENABLE_SANITIZER_MISC                           OFF) # No other misc sanitizations
    set(SKL_LTO                      THIN CACHE STRING "" FORCE) # Thin LTO

    message("-- [SKL] STAGING BUILD!")
elseif(SKL_BUILD_TYPE STREQUAL "DEV")
    if((NOT CMAKE_BUILD_TYPE STREQUAL "Debug") AND (NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
        message(FATAL_ERROR "DEV must be built in RelWithDebInfo or Debug!")
    endif()

    set(SKL_LIB_LOG_LEVEL            DEBUG CACHE STRING "" FORCE) # DEBUG log level
    set(SKL_NO_ASSERTS                                       OFF) # No asserts 
    set(SKL_ENABLE_SANITIZER_ADDRESS                          ON) # No asan
    set(SKL_ENABLE_SANITIZER_THREAD                          OFF) # No tsan
    set(SKL_ENABLE_SANITIZER_LEAK                            OFF) # No lsan
    set(SKL_ENABLE_SANITIZER_UB                              OFF) # No ubsan
    set(SKL_ENABLE_SANITIZER_MISC                            OFF) # No other misc sanitizations
    set(SKL_LTO                       NONE CACHE STRING "" FORCE) # No LTO

    message("-- [SKL] DEV BUILD!")
else()
    message(FATAL_ERROR "-- [SKL] UNKNOWN BUILD!")
endif()

# Name of the development skl targets
set(SKL_DEV_NAME "_dev" CACHE STRING "[DEV] Name of the development skl targets"  FORCE)

# Set properties options
set_property(CACHE SKL_L1_CACHE_LINE_SIZE PROPERTY STRINGS ${SKL_L1_CACHE_LINE_SIZE_OPTIONS}) # SKL_L1_CACHE_LINE_SIZE
set_property(CACHE SKL_REAL_TYPE          PROPERTY STRINGS ${SKL_REAL_TYPE_OPTIONS})          # SKL_REAL_TYPE
set_property(CACHE SKL_LIB_LOG_LEVEL      PROPERTY STRINGS ${SKL_LOG_LEVEL_OPTIONS})          # SKL_LOG_LEVEL
set_property(CACHE SKL_LTO                PROPERTY STRINGS ${SKL_LTO_OPTIONS})                # LTO
set_property(CACHE SKL_BUILD_TYPE         PROPERTY STRINGS ${SKL_BUILD_TYPE_OPTIONS})         # Build type

# Dev test targets require testing to be enabled
if(SKL_ENABLE_DEV_TEST_TARGETS AND NOT SKL_ENABLE_TESTS)
    message(FATAL_ERROR "SKL_ENABLE_DEV_TEST_TARGETS=ON requires SKL_ENABLE_TESTS=ON")
endif()

# Code coverage require testing to be enabled
if(SKL_ENABLE_CODE_COVERAGE AND NOT SKL_ENABLE_TESTS)
    message(FATAL_ERROR "SKL_ENABLE_CODE_COVERAGE=ON requires SKL_ENABLE_TESTS=ON")
endif()

# Enable cmake testing
if(SKL_ENABLE_DEV_TEST_TARGETS)
    enable_testing()
endif()

# No sanitizers while collecting code coverage
if(SKL_ENABLE_CODE_COVERAGE)
    if(SKL_ENABLE_SANITIZER_ADDRESS
    OR SKL_ENABLE_SANITIZER_THREAD
    OR SKL_ENABLE_SANITIZER_LEAK
    OR SKL_ENABLE_SANITIZER_UB
    OR SKL_ENABLE_SANITIZER_MISC)
        message(FATAL_ERROR "Disable sanitization when building for code coverage!")
    endif()
endif()

if(SKL_ENABLE_TESTS)
    set(SKL_ENABLE_SKL_NET_TOOLS ON)
endif()
