//!
//! \file skl_guid
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>
#include <cstring>

#include "skl_guid"
#include "skl_sguid"
#include "skl_sguid64"
#include "skl_rand"

namespace {
thread_local char g_string_buffer[64u];
}

namespace skl {
GUID make_guid() noexcept {
    SklRand& rand{get_thread_rand()};
    byte     rand_bytes[GUID::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

GUID make_guid_fast() noexcept {
    SklRand&  rand{get_thread_rand()};
    const u64 lo = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    const u64 hi = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    return {lo, hi};
}

GUID g_make_guid() noexcept {
    SklRand rand{};
    byte    rand_bytes[GUID::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

GUID g_make_guid_fast() noexcept {
    SklRand   rand{};
    const u64 lo = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    const u64 hi = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    return {lo, hi};
}

SGUID make_sguid() noexcept {
    SklRand& rand{get_thread_rand()};
    byte     rand_bytes[SGUID::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

SGUID make_sguid_fast() noexcept {
    SklRand& rand{get_thread_rand()};
    return {rand.next()};
}

SGUID g_make_sguid() noexcept {
    SklRand rand{};
    byte    rand_bytes[SGUID::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

SGUID g_make_sguid_fast() noexcept {
    SklRand rand{};
    return {rand.next()};
}

u64 GUID::to_string(skl_buffer_view f_target_buffer) const noexcept {
    // Note: snprintf can return negative on encoding errors, but this cannot happen here:
    // - Format strings are hardcoded and valid
    // - Arguments are simple unsigned integers formatted as hex
    // - No wide character conversions or locale-dependent behavior
    // Therefore, we safely assume non-negative return values

    if (is_null()) {
        const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                                  f_target_buffer.length,
                                  "00000000000000000000000000000000");
        return static_cast<u64>(len);
    }

    const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                              f_target_buffer.length,
                              "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                              static_cast<unsigned>(m_low & 0xffu),
                              static_cast<unsigned>((m_low >> 8u) & 0xffu),
                              static_cast<unsigned>((m_low >> 16u) & 0xffu),
                              static_cast<unsigned>((m_low >> 24u) & 0xffu),
                              static_cast<unsigned>((m_low >> 32u) & 0xffu),
                              static_cast<unsigned>((m_low >> 40u) & 0xffu),
                              static_cast<unsigned>((m_low >> 48u) & 0xffu),
                              static_cast<unsigned>((m_low >> 56u) & 0xffu),
                              static_cast<unsigned>(m_high & 0xffu),
                              static_cast<unsigned>((m_high >> 8u) & 0xffu),
                              static_cast<unsigned>((m_high >> 16u) & 0xffu),
                              static_cast<unsigned>((m_high >> 24u) & 0xffu),
                              static_cast<unsigned>((m_high >> 32u) & 0xffu),
                              static_cast<unsigned>((m_high >> 40u) & 0xffu),
                              static_cast<unsigned>((m_high >> 48u) & 0xffu),
                              static_cast<unsigned>((m_high >> 56u) & 0xffu));
    return static_cast<u64>(len);
}

skl_string_view GUID::to_string() const noexcept {
    skl_buffer_view buffer_view{g_string_buffer};
    buffer_view.position = to_string(buffer_view);
    return skl_string_view::exact(g_string_buffer, buffer_view.position);
}

u64 GUID::to_string_fancy(skl_buffer_view f_target_buffer) const noexcept {
    // Note: snprintf can return negative on encoding errors, but this cannot happen here:
    // - Format strings are hardcoded and valid
    // - Arguments are simple unsigned integers formatted as hex
    // - No wide character conversions or locale-dependent behavior
    // Therefore, we safely assume non-negative return values

    if (is_null()) {
        const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                                  f_target_buffer.length,
                                  "00000000-0000-0000-0000-000000000000");
        return static_cast<u64>(len);
    }

    const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                              f_target_buffer.length,
                              "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                              static_cast<unsigned>(m_low & 0xffu),
                              static_cast<unsigned>((m_low >> 8u) & 0xffu),
                              static_cast<unsigned>((m_low >> 16u) & 0xffu),
                              static_cast<unsigned>((m_low >> 24u) & 0xffu),
                              static_cast<unsigned>((m_low >> 32u) & 0xffu),
                              static_cast<unsigned>((m_low >> 40u) & 0xffu),
                              static_cast<unsigned>((m_low >> 48u) & 0xffu),
                              static_cast<unsigned>((m_low >> 56u) & 0xffu),
                              static_cast<unsigned>(m_high & 0xffu),
                              static_cast<unsigned>((m_high >> 8u) & 0xffu),
                              static_cast<unsigned>((m_high >> 16u) & 0xffu),
                              static_cast<unsigned>((m_high >> 24u) & 0xffu),
                              static_cast<unsigned>((m_high >> 32u) & 0xffu),
                              static_cast<unsigned>((m_high >> 40u) & 0xffu),
                              static_cast<unsigned>((m_high >> 48u) & 0xffu),
                              static_cast<unsigned>((m_high >> 56u) & 0xffu));
    return static_cast<u64>(len);
}

skl_string_view GUID::to_string_fancy() const noexcept {
    skl_buffer_view buffer_view{g_string_buffer};
    buffer_view.position = to_string_fancy(buffer_view);
    return skl_string_view::exact(g_string_buffer, buffer_view.position);
}

u64 SGUID::to_string(skl_buffer_view f_target_buffer) const noexcept {
    // Note: snprintf can return negative on encoding errors, but this cannot happen here:
    // - Format strings are hardcoded and valid
    // - Arguments are simple unsigned integers formatted as hex
    // - No wide character conversions or locale-dependent behavior
    // Therefore, we safely assume non-negative return values

    if (is_null()) {
        const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                                  f_target_buffer.length,
                                  "00000000");
        return static_cast<u64>(len);
    }
    const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                              f_target_buffer.length,
                              "%02x%02x%02x%02x",
                              static_cast<unsigned>((*this)[0]),
                              static_cast<unsigned>((*this)[1]),
                              static_cast<unsigned>((*this)[2]),
                              static_cast<unsigned>((*this)[3]));
    return static_cast<u64>(len);
}

skl_string_view SGUID::to_string() const noexcept {
    skl_buffer_view buffer_view{g_string_buffer};
    buffer_view.position = to_string(buffer_view);
    return skl_string_view::exact(g_string_buffer, buffer_view.position);
}

SGUID64 make_sguid64() noexcept {
    SklRand& rand{get_thread_rand()};
    byte     rand_bytes[SGUID64::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

SGUID64 make_sguid64_fast() noexcept {
    SklRand&  rand{get_thread_rand()};
    const u64 value = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    return {value};
}

SGUID64 g_make_sguid64() noexcept {
    SklRand rand{};
    byte    rand_bytes[SGUID64::CSize];
    for (auto& rand_byte : rand_bytes) {
        rand_byte = rand.next_range(0U, 0xFFU);
    }
    return {rand_bytes};
}

SGUID64 g_make_sguid64_fast() noexcept {
    SklRand   rand{};
    const u64 value = static_cast<u64>(rand.next()) | (static_cast<u64>(rand.next()) << 32u);
    return {value};
}

u64 SGUID64::to_string(skl_buffer_view f_target_buffer) const noexcept {
    // Note: snprintf can return negative on encoding errors, but this cannot happen here:
    // - Format strings are hardcoded and valid
    // - Arguments are simple unsigned integers formatted as hex
    // - No wide character conversions or locale-dependent behavior
    // Therefore, we safely assume non-negative return values

    if (is_null()) {
        const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                                  f_target_buffer.length,
                                  "0000000000000000");
        return static_cast<u64>(len);
    }
    const auto len = snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                              f_target_buffer.length,
                              "%02x%02x%02x%02x%02x%02x%02x%02x",
                              static_cast<unsigned>((*this)[0]),
                              static_cast<unsigned>((*this)[1]),
                              static_cast<unsigned>((*this)[2]),
                              static_cast<unsigned>((*this)[3]),
                              static_cast<unsigned>((*this)[4]),
                              static_cast<unsigned>((*this)[5]),
                              static_cast<unsigned>((*this)[6]),
                              static_cast<unsigned>((*this)[7]));
    return static_cast<u64>(len);
}

skl_string_view SGUID64::to_string() const noexcept {
    skl_buffer_view buffer_view{g_string_buffer};
    buffer_view.position = to_string(buffer_view);
    return skl_string_view::exact(g_string_buffer, buffer_view.position);
}
} // namespace skl
