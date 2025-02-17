cmake_minimum_required (VERSION 3.8)
project ("SkylakeLib" VERSION 1.0.0 LANGUAGES CXX C)

# C++ 23
set(CMAKE_CXX_STANDARD 23) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Update modules path
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(SkylakeLib_Utils)
include(SkylakeLib_Options)
include(SkylakeLib_Platform)
include(SkylakeLib_CompilerFlags)
include(SkylakeLib_Warnings)
include(SkylakeLib_CreateTargets)
include(SkylakeLib_Tuning)

################################################################

add_subdirectory("third_party")

add_subdirectory("skl-core")

################################################################

if(SKL_ENABLE_TESTS)
    add_subdirectory("tests")
endif()

if(SKL_DEV)
    skl_CreateDevTargets()
endif()
