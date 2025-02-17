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
struct thread_id_t {
    thread_id_t() noexcept {
        SklRand rand{};
        id = rand.next();
    }

    u32 id{0U};
};
} // namespace skl
SKL_MAKE_TLS_SINGLETON(skl::thread_id_t, g_thread_id);
static_assert(sizeof(skl::thread_id_t) <= 8U);

namespace skl {
bool init_current_thread_id() noexcept {
    return g_thread_id::tls_create().is_success();
}
unsigned int current_thread_id() noexcept {
    return g_thread_id::tls_guarded().id;
}
unsigned int current_thread_id_unsafe() noexcept {
    return g_thread_id::tls_checked().id;
}
} // namespace skl
