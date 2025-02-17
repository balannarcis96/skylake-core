include_guard()

include(CheckTypeSize)

# Global "generated" flags
set(SKL_PLATFORM_DETECTED  "FALSE"   CACHE STRING "[Generated] Has the platform been detected?" FORCE)
set(SKL_BUILD_OS           "UNKNOWN" CACHE STRING "[Generated] SkylakeLib target platform" FORCE)
set(SKL_MEM_PAGE_SIZE      "UNKNWON" CACHE STRING "[Generated] Platform memory page size" FORCE)
set(SKL_MEM_HUGE_PAGE_SIZE "UNKNWON" CACHE STRING "[Generated] Platform memory huge page size" FORCE)

# Private
function( _skl_DetectPlatform )

    # Assert 64bit
    check_type_size("void*" LOCAL_VOID_PTR_SIZE)
    if(NOT(LOCAL_VOID_PTR_SIZE STREQUAL "8"))
        message(FATAL_ERROR "Only 64bit platforms are supported!")
    endif()

    # Assert x86 arch
    if (NOT(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)"))
        message(FATAL_ERROR "Only x86 64bit platforms are supported!")
    endif()

    if(NOT "${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Linux")
        message(FATAL_ERROR "Only Linux is supported!")
    endif()

    # Update the global flag
    set(SKL_PLATFORM_DETECTED "TRUE" CACHE STRING "" FORCE)

    # Assert compiler
    _skl_AssertCompiler()

    # Assert cpp 23
    _skl_AssertCpp23()

endfunction()

# Private
function( _skl_AssertCpp23 )

    if(NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD LESS 23)
        message(FATAL_ERROR "[SKL] C++23 is required.")
    endif()
  
endfunction()

# Private
function( _skl_AssertCompiler )

    # Current only clang is supported
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Only Clang is supported!")
    endif()

endfunction()

##################################################
# Detect the platform and query necessary values #
##################################################
_skl_DetectPlatform()
