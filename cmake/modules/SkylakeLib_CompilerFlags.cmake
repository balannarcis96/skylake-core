include_guard()

# Variables
set(SKL_SANITIZATION_LIST "" CACHE STRING "" FORCE)
set(SKL_DEFINITIONS_LIST "" CACHE STRING "" FORCE)
set(SKL_COMPILE_FLAGS_LIST "" CACHE STRING "" FORCE)
set(SKL_LINK_FLAGS_LIST "" CACHE STRING "" FORCE)
set(SKL_ENABLE_LIB_TESTING_UTILS "FALSE" CACHE STRING "" FORCE)

# Set DWARF to version 4
skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-gdwarf-4")

# Add pthread support
skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-pthread")
skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-pthread")

# SKL_NOINLINE
add_compile_definitions(SKL_NOINLINE=[[clang::noinline]])

# _DEBUG def
if(CMAKE_BUILD_TYPE MATCHES Debug)
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "_DEBUG=1")
endif()

# SKL_BUILD_TYPE
if(SKL_BUILD_TYPE STREQUAL "DEV")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_BUILD_DEV=1")
elseif(SKL_BUILD_TYPE STREQUAL "STAGING")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_BUILD_STAGING=1")
elseif(SKL_BUILD_TYPE STREQUAL "SHIPPING")
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-finline-functions")   # Aggressive inlining
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-fno-stack-protector") # [Disabled] Detects some buffer overruns that overwrite a function's return address
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-ffast-math")          # Floating point - fast
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-fno-rtti")            # Disable RTTI

    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_BUILD_SHIPPING=1")
endif()

# Wall
if(SKL_WALL)
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-Wall")
endif()

# Werror
if(SKL_WERROR)
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-Werror")
endif()

# LTO
if(SKL_LTO STREQUAL STANDARD)
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-flto")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST    "-flto")
elseif(SKL_LTO STREQUAL THIN)
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-flto=thin")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST    "-flto=thin")
elseif(SKL_LTO STREQUAL FULL)
    skl_AppendToCachedList(SKL_COMPILE_FLAGS_LIST "-flto=full")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST    "-flto=full")
endif()

# SKL_LIB_LOG_LEVEL
if(SKL_LIB_LOG_LEVEL STREQUAL "DEBUG")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=5")
elseif(SKL_LIB_LOG_LEVEL STREQUAL "INFO")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=4")
elseif(SKL_LIB_LOG_LEVEL STREQUAL "WARNING")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=3")
elseif(SKL_LIB_LOG_LEVEL STREQUAL "ERROR")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=2")
elseif(SKL_LIB_LOG_LEVEL STREQUAL "FATAL")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=1")
else()
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_LIB_LOG_LEVEL=0")
endif()

# SKL_NO_ASSERTS
if(SKL_NO_ASSERTS)
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_NO_ASSERTS=1")
else()
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_NO_ASSERTS=0")
endif()

# SANITIZERS
if(SKL_ENABLE_SANITIZER_ADDRESS)
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=address")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=address")
    # skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=memory")
    # skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=memory")
endif()
if(SKL_ENABLE_SANITIZER_THREAD)
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=thread")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=thread")
endif()
if(SKL_ENABLE_SANITIZER_LEAK AND SKL_BUILD_TYPE STREQUAL "STAGING")
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=leak")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=leak")
endif()
if(SKL_ENABLE_SANITIZER_UB)
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=undefined")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=undefined")
endif()
if(SKL_ENABLE_SANITIZER_MISC)
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=return")
    skl_AppendToCachedList(SKL_SANITIZATION_LIST "-fsanitize=bounds")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=return")
    skl_AppendToCachedList(SKL_LINK_FLAGS_LIST "-fsanitize=bounds")
endif()

# SKL_ENABLE_LIKELY_FLAGS
if(SKL_ENABLE_LIKELY_FLAGS)
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_ENABLE_LIKELY_FLAGS=1")
endif()

# SKL_ENABLE_UNLIKELY_FLAGS
if(SKL_ENABLE_UNLIKELY_FLAGS)
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_ENABLE_UNLIKELY_FLAGS=1")
endif()

# SKL_REAL_TYPE
if(SKL_REAL_TYPE STREQUAL "Double")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_REAL_TYPE_DOUBLE=1")
else()
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_REAL_TYPE_DOUBLE=0")
endif()

# SKL_L1_CACHE_LINE_SIZE
if(SKL_L1_CACHE_LINE_SIZE STREQUAL "64bytes")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_LINE_SIZE=64")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_ALIGNED=alignas(64)")
elseif(SKL_L1_CACHE_LINE_SIZE STREQUAL "128bytes")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_LINE_SIZE=128")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_ALIGNED=alignas(128)")
elseif(SKL_L1_CACHE_LINE_SIZE STREQUAL "512bytes")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_LINE_SIZE=512")
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_CACHE_ALIGNED=alignas(512)")
endif()

if(SKL_ENABLE_DEV_TEST_TARGETS)
    skl_AppendToCachedList(SKL_DEFINITIONS_LIST "SKL_ENABLE_TESTING_UTILS=1")
endif()
