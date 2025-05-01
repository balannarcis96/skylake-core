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
struct _ThreadId {
    _ThreadId() noexcept {
        SklRand rand{};
        ThreadId = rand.next();
    }

    u32 ThreadId{0U};
};
} // namespace skl
SKL_MAKE_TLS_SINGLETON(skl::_ThreadId, g_thread_id);
static_assert(sizeof(skl::_ThreadId) <= 8U);

namespace {
SKL_NOINLINE void skl_thread_id_create() noexcept {
    SKL_ASSERT_PERMANENT(g_thread_id::tls_create().is_success());
}
} // namespace

namespace skl {
unsigned int current_thread_id() noexcept {
    if (false == g_thread_id::tls_init_status()) [[unlikely]] {
        skl_thread_id_create();
    }

    return g_thread_id::tls_guarded().ThreadId;
}
} // namespace skl
