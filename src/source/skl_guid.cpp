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
    SklRand& rand{get_thread_rand()};
    byte     rand_bytes[GUID::CSize];

    *reinterpret_cast<u32*>(rand_bytes)       = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 4U)  = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 8U)  = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 12U) = rand.next();

    static_assert((sizeof(u32) * 4U) == GUID::CSize);

    return {rand_bytes};
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
    SklRand rand{};
    byte    rand_bytes[GUID::CSize];

    *reinterpret_cast<u32*>(rand_bytes)       = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 4U)  = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 8U)  = rand.next();
    *reinterpret_cast<u32*>(rand_bytes + 12U) = rand.next();

    static_assert((sizeof(u32) * 4U) == GUID::CSize);

    return {rand_bytes};
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
    byte     rand_bytes[SGUID::CSize];

    *reinterpret_cast<u32*>(rand_bytes) = rand.next();

    static_assert(sizeof(u32) == SGUID::CSize);

    return {rand_bytes};
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
    byte    rand_bytes[SGUID::CSize];

    *reinterpret_cast<u32*>(rand_bytes) = rand.next();

    static_assert(sizeof(u32) == SGUID::CSize);

    return {rand_bytes};
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
                   m_data[0U],
                   m_data[1U],
                   m_data[2U],
                   m_data[3U],
                   m_data[4U],
                   m_data[5U],
                   m_data[6U],
                   m_data[7U],
                   m_data[8U],
                   m_data[9U],
                   m_data[10U],
                   m_data[11U],
                   m_data[12U],
                   m_data[13U],
                   m_data[14U],
                   m_data[15U]);
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
                   m_data[0U],
                   m_data[1U],
                   m_data[2U],
                   m_data[3U]);
}
} // namespace skl
