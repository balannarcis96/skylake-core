//!
//! \file skl_rand
//!
//! \source https://youtu.be/LWFzPP8ZbdU?t=2817
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <tune_skl_core_public.h>

#include "skl_rand"
#include "skl_tls"
#include "skl_epoch"

namespace skl::skl_rand_internals {
u32 skl_rand(rand_position_t f_pos, rand_seed_t f_seed) noexcept {
    constexpr u32 CNoise_1 = Squirrel1_NOISE1;
    constexpr u32 CNoise_2 = Squirrel1_NOISE2;
    constexpr u32 CNoise_3 = Squirrel1_NOISE3;

    auto result = f_pos;

    //Apply noise pass 1
    result *= CNoise_1;
    result ^= (result >> 8U);

    //Apply seed
    result += f_seed;

    //Apply noise pass 2
    result += CNoise_2;
    result ^= (result << 8U);

    //Apply noise pass 3
    result *= CNoise_3;
    result ^= (result >> 8U);

    return result;
}

u32 skl_rand_2d(i32 f_x, i32 f_y, rand_seed_t f_seed) noexcept {
    constexpr u32 CPrime{Squirrel3_2D_PRIME};
    return skl_rand(f_x + (i32(CPrime) * f_y), f_seed);
}

u32 skl_rand_3d(i32 f_x, i32 f_y, i32 f_z, rand_seed_t f_seed) noexcept {
    constexpr u32 CPrime_1{Squirrel3_3D_PRIME1};
    constexpr u32 CPrime_2{Squirrel3_3D_PRIME2};
    return skl_rand(f_x + (i32(CPrime_1) * f_y) + (i32(CPrime_2) * f_z), f_seed);
}
} // namespace skl::skl_rand_internals

SKL_MAKE_TLS_SINGLETON(skl::SklRand, TLSRand)

namespace skl {
rand_position_t SklRand::new_seed() noexcept {
    //Use the time value as the seed
    const auto current_epoch_time = get_current_epoch_time();

    m_seed     = static_cast<rand_seed_t>(current_epoch_time + __rdtsc());
    m_position = 1U;

    return m_position;
}

rand_position_t SklRand::pos() noexcept {
    //If reached max position for this seed, reseed
    if ((m_position + 1U) == 0xFFFFFFFFU) {
        return new_seed();
    }

    [[likely]] return ++m_position;
}

SklRand& get_thread_rand() noexcept {
    return TLSRand::tls_guarded();
}
} // namespace skl

namespace skl {
skl_status skl_core_init_thread__rand() noexcept {
    if (TLSRand::tls_create().is_failure()) {
        return SKL_ERR_TLS_INIT;
    }

    return SKL_SUCCESS;
}
skl_status skl_core_deinit_thread__rand() noexcept {
    TLSRand::tls_destroy();
    return SKL_SUCCESS;
}
} // namespace skl
