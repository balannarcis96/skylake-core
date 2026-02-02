//!
//! \file skl_guid
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>
#include <cstring>

#include "skl_guid"
#include "skl_sguid"
#include "skl_rand"

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

void GUID::to_string(skl_buffer_view f_target_buffer) const noexcept {
    if (is_null()) {
        std::strncpy(reinterpret_cast<char*>(f_target_buffer.buffer),
                     "00000000-0000-0000-0000-000000000000",
                     f_target_buffer.length);
        return;
    }

    (void)snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
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
}

void SGUID::to_string(skl_buffer_view f_target_buffer) const noexcept {
    if (is_null()) {
        std::strncpy(reinterpret_cast<char*>(f_target_buffer.buffer),
                     "00000000",
                     f_target_buffer.length);
        return;
    }
    (void)snprintf(reinterpret_cast<char*>(f_target_buffer.buffer),
                   f_target_buffer.length,
                   "%02x%02x%02x%02x",
                   static_cast<unsigned>((*this)[0]),
                   static_cast<unsigned>((*this)[1]),
                   static_cast<unsigned>((*this)[2]),
                   static_cast<unsigned>((*this)[3]));
}
} // namespace skl