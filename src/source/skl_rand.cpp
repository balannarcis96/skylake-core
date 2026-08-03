//!
//! \file skl_rand
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <sys/random.h>

#include <tune_skl_core_public.h>

#include "skl_rand"
#include "skl_tls"
#include "skl_epoch"

namespace skl::skl_rand_internals {
u64 skl_rand(rand_position_t f_pos, rand_seed_t f_seed) noexcept {
    //Odd stride -> bijective in the position, so this agrees exactly with SklRand::next_u64()
    //walking the same stream (state_n == seed + n * stride)
    static_assert(1ULL == (SklRandStride & 1ULL), "SklRandStride must be odd");

    return skl_finalize((f_pos * SklRandStride) + f_seed);
}

u64 skl_rand_2d(i32 f_x, i32 f_y, rand_seed_t f_seed) noexcept {
    constexpr u64 CPrime{SklRand2D_PRIME};

    const auto x = static_cast<u64>(static_cast<i64>(f_x));
    const auto y = static_cast<u64>(static_cast<i64>(f_y));

    return skl_rand(x + (CPrime * y), f_seed);
}

u64 skl_rand_3d(i32 f_x, i32 f_y, i32 f_z, rand_seed_t f_seed) noexcept {
    constexpr u64 CPrime_1{SklRand3D_PRIME1};
    constexpr u64 CPrime_2{SklRand3D_PRIME2};

    const auto x = static_cast<u64>(static_cast<i64>(f_x));
    const auto y = static_cast<u64>(static_cast<i64>(f_y));
    const auto z = static_cast<u64>(static_cast<i64>(f_z));

    return skl_rand(x + (CPrime_1 * y) + (CPrime_2 * z), f_seed);
}
} // namespace skl::skl_rand_internals

SKL_MAKE_TLS_SINGLETON(skl::SklRand, TLSRand)

namespace skl {
void SklRand::new_seed() noexcept {
    rand_seed_t state;

    //Kernel CSPRNG - a full 64 bits of real entropy, so distinct streams only collide at the
    //2^32 birthday bound instead of the ~2^16 the clock+tsc fallback is worth
    if (static_cast<ssize_t>(sizeof(state)) != ::getrandom(&state, sizeof(state), GRND_NONBLOCK)) {
        //Entropy pool not ready yet (very early boot) - fall back to the clock and the tsc
        constexpr u64 CGoldenRatio = 0x9E3779B97F4A7C15ULL;
        state = (static_cast<u64>(get_current_epoch_time()) * CGoldenRatio) ^ static_cast<u64>(__rdtsc());
    }

    m_state = state;
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
