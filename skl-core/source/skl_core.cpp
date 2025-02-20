//!
//! \file skl_core
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_core"
#include "skl_thread_id"
#include "skl_signal"
#include "skl_assert"
#include "skl_atomic"

namespace {
//Is the skl core initialized on the current thread
thread_local bool g_is_skl_core_init_on_thread{false};

//Is the skl core initialized
SKL_CACHE_ALIGNED std::relaxed_value<bool> g_is_skl_core_init{false};
} // namespace

namespace skl {
extern skl_status skl_core_init_thread__slog() noexcept;
extern skl_status skl_core_init_thread__rand() noexcept;

extern skl_status skl_core_init_thread__slog_bend() noexcept;
extern skl_status skl_core_deinit_thread__slog_bend() noexcept;

extern skl_status skl_core_deinit_thread__slog() noexcept;
extern skl_status skl_core_deinit_thread__rand() noexcept;
} // namespace skl

namespace skl {
skl_status skl_core_init() noexcept {
    if (g_is_skl_core_init.exchange(true)) {
        return SKL_OK_REDUNDANT;
    }

    if (skl_core_init_thread().is_failure()) {
        (void)g_is_skl_core_init.exchange(false);
        return SKL_ERR_FAIL;
    }

    puts("SKL_CORE_INIT!");
    return SKL_SUCCESS;
}

skl_status skl_core_init_thread() noexcept {
    if (g_is_skl_core_init_on_thread) {
        return SKL_OK_REDUNDANT;
    }

    SKL_ASSERT_PERMANENT(init_program_epilog().is_success());

    if (false == init_current_thread_id()) {
        return SKL_ERR_INIT_ID;
    }

    if (skl_core_init_thread__slog().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

    if (skl_core_init_thread__slog_bend().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

    if (skl_core_init_thread__rand().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

    g_is_skl_core_init_on_thread = true;

#if 0
    puts("SKL_CORE_INIT_THREAD!");
#endif
    return SKL_SUCCESS;
}

skl_status skl_core_deinit_thread() noexcept {
    if (false == g_is_skl_core_init_on_thread) {
        return SKL_OK_REDUNDANT;
    }

    g_is_skl_core_init_on_thread = false;

    if (skl_core_deinit_thread__rand().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

    if (skl_core_deinit_thread__slog_bend().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

    if (skl_core_deinit_thread__slog().is_failure()) {
        return SKL_ERR_INIT_LOG;
    }

#if 0
    puts("SKL_CORE_DEINIT_THREAD!");
#endif
    return SKL_SUCCESS;
}

skl_status skl_core_deinit() noexcept {
    if (false == g_is_skl_core_init.exchange(false)) {
        return SKL_OK_REDUNDANT;
    }

    if (skl_core_deinit_thread().is_failure()) {
        return SKL_ERR_FAIL;
    }

    puts("SKL_CORE_DEINIT!");
    return SKL_SUCCESS;
}
} // namespace skl
