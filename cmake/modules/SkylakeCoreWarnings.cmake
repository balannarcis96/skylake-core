#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
include_guard()

# Variables
set(SKL_CORE_GENERAL_DISABLED_WARNINGS "" CACHE STRING "" FORCE)
set(SKL_CORE_TESTS_DISABLED_WARNINGS "" CACHE STRING "" FORCE)

function( skl_AddDsiableWarning 
          WARNING )

    skl_AppendToCachedList(SKL_CORE_GENERAL_DISABLED_WARNINGS ${WARNING})

endfunction()

function( skl_AddTestsDsiableWarning 
          WARNING )

    skl_AppendToCachedList(SKL_CORE_TESTS_DISABLED_WARNINGS ${WARNING})

endfunction()

# Disabled warnings
skl_AddDsiableWarning("-Wno-gnu-anonymous-struct")
skl_AddDsiableWarning("-Wno-header-hygiene")
skl_AddDsiableWarning("-Wno-newline-eof")
skl_AddDsiableWarning("-Wno-nested-anon-types")
skl_AddDsiableWarning("-Wno-shadow")
skl_AddDsiableWarning("-Wno-undef")
skl_AddDsiableWarning("-Wno-double-promotion")
skl_AddDsiableWarning("-Wno-missing-prototypes")
skl_AddDsiableWarning("-Wno-c++98-compat-pedantic")
skl_AddDsiableWarning("-Wno-extra-semi-stmt")
skl_AddDsiableWarning("-Wno-global-constructors")
skl_AddDsiableWarning("-Wno-nonportable-system-include-path")
skl_AddDsiableWarning("-Wno-shadow-uncaptured-local")
skl_AddDsiableWarning("-Wno-float-equal")
skl_AddDsiableWarning("-Wno-old-style-cast")
skl_AddDsiableWarning("-Wno-shadow-field-in-constructor")
skl_AddDsiableWarning("-Wno-format-non-iso")
skl_AddDsiableWarning("-Wno-documentation-unknown-command")
skl_AddDsiableWarning("-Wno-zero-as-null-pointer-constant")
skl_AddDsiableWarning("-Wno-documentation")
skl_AddDsiableWarning("-Wno-missing-variable-declarations")
skl_AddDsiableWarning("-Wno-suggest-override")
skl_AddDsiableWarning("-Wno-deprecated-dynamic-exception-spec")
skl_AddDsiableWarning("-Wno-sign-conversion")
skl_AddDsiableWarning("-Wno-ctad-maybe-unsupported")
skl_AddDsiableWarning("-Wno-reserved-identifier")
skl_AddDsiableWarning("-Wno-exit-time-destructors")
skl_AddDsiableWarning("-Wno-format-security")
skl_AddDsiableWarning("-Wno-unused-macros")
skl_AddDsiableWarning("-Wno-undefined-reinterpret-cast")
skl_AddDsiableWarning("-Wno-unused-const-variable")
skl_AddDsiableWarning("-Wno-gnu-zero-variadic-macro-arguments")
skl_AddDsiableWarning("-Wno-c++20-compat")
skl_AddDsiableWarning("-Wno-format-security")
skl_AddDsiableWarning("-Wno-undefined-var-template")

if(SKL_ENABLE_TESTS)
    skl_AddTestsDsiableWarning("-Wno-deprecated")
    skl_AddTestsDsiableWarning("-Wno-suggest-destructor-override")
    skl_AddTestsDsiableWarning("-Wno-used-but-marked-unused")
    skl_AddTestsDsiableWarning("-Wno-switch-enum")
    skl_AddTestsDsiableWarning("-Wno-missing-noreturn")
    skl_AddTestsDsiableWarning("-Wno-unused-member-function")
endif()
