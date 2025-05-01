//!
//! \file skl_validate
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <skl_int>
#include <skl_type>
#include <skl_utility>

#include <tune_skl_core_private.h>
#include <tune_skl_core_public.h>

/*
 * CORE ASSERTIONS!
 * This library was designed to be compiled for: Linux - x86-64bit and with: Clang 19+
 * 
 * Motivation: Having the platform, OS and compiler be fixed allowes for:
 *              - faster dev
 *              - faster code
 *              - faster cmpilation
 *              - smaller footprint
 *              - simpler code
 *              - less testing
 *              - more deterministic
 *              - easier mentenance
 *              - less dependencies
 *               - eg. less dependency on the std lib leading to faster compilation times (smaller headers)
 */
static_assert(sizeof(char) == 1U);
static_assert(sizeof(unsigned char) == 1U);
static_assert(alignof(char) == 1U);
static_assert(alignof(unsigned char) == 1U);

static_assert(sizeof(short) == 2U);
static_assert(sizeof(unsigned short) == 2U);
static_assert(alignof(short) == 2U);
static_assert(alignof(unsigned short) == 2U);

static_assert(sizeof(int) == 4U);
static_assert(sizeof(unsigned int) == 4U);
static_assert(alignof(int) == 4U);
static_assert(alignof(unsigned int) == 4U);

static_assert(sizeof(long) == 8U);
static_assert(sizeof(unsigned long) == 8U);
static_assert(alignof(long) == 8U);
static_assert(alignof(unsigned long) == 8U);

static_assert(sizeof(long long) == 8U);
static_assert(sizeof(unsigned long long) == 8U);
static_assert(alignof(long long) == 8U);
static_assert(alignof(unsigned long long) == 8U);

static_assert(sizeof(void*) == 8U);
static_assert(alignof(void*) == 8U);
static_assert('A' == 65, "Character set must be ASCII-compatible");

#if !defined(__clang__)
#    error "Compiler is not Clang"
#endif

static_assert(__clang_major__ >= 19, "Clang 19+ is required");

#if __cplusplus < 202302L
#    error "C++23 or later is required"
#endif

static_assert(sizeof(i8) == 1U);
static_assert(sizeof(u8) == 1U);
static_assert(sizeof(i16) == 2U);
static_assert(sizeof(u16) == 2U);
static_assert(sizeof(i32) == 4U);
static_assert(sizeof(u32) == 4U);
static_assert(sizeof(i64) == 8U);
static_assert(sizeof(u64) == 8U);

static_assert(sizeof(skl::TDuration) == 4U);
static_assert(sizeof(skl::TTimePoint) == 4U);
static_assert(sizeof(skl::TSystemTimePoint) == 8U);
static_assert(sizeof(skl::TEpochTimePoint) == 8U);
static_assert(sizeof(skl::TEpochTimeDuration) == 8U);

static_assert(skl::next_power_of_2<u32(1000)>() == 1024);
static_assert(skl::next_power_of_2<u64(1024)>() == 1024);
static_assert(skl::next_power_of_2<u32(1025)>() == 2048);
static_assert(skl::next_power_of_2<u32>(1000) == 1024);
static_assert(skl::next_power_of_2<u64>(50000) == 65536);

static_assert(skl::CSerializedLoggerThreadBufferSize >= 4096U, "CSerializedLoggerThreadBufferSize min = 4096");
static_assert(0ULL == u64(skl::CSklReportingThreadBufferSize & (skl::CSklReportingThreadBufferSize - 1ULL)), "CSklReportingThreadBufferSize must be a power of 2");
