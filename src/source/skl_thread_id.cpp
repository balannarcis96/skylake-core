//!
//! \file skl_thread_id
//!
//! \brief thread uuid untility
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_thread_id"
#include "skl_tls"
#include "skl_rand"

namespace skl {
struct skl_core_thread_id_t {
    skl_core_thread_id_t() noexcept {
        SklRand rand{};
        ThreadId = rand.next();
    }

    u32 ThreadId{0U};
};
} // namespace skl
SKL_MAKE_TLS_SINGLETON(skl::skl_core_thread_id_t, g_skl_core_thread_id);
static_assert(sizeof(skl::skl_core_thread_id_t) <= 8U);

namespace skl {
thread_id_t current_thread_id() noexcept {
    return g_skl_core_thread_id::tls_guarded().ThreadId;
}
} // namespace skl
