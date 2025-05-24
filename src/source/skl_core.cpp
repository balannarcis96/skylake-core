//!
//! \file skl_core
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>

#include "skl_core"
#include "skl_signal"
#include "skl_assert"
#include "skl_atomic"
#include "skl_fixed_vector_if"
#include "skl_core_info"
#include "skl_thread"

namespace {
//Is the skl core initialized on the current thread
thread_local bool g_is_skl_core_init_on_thread{false};

//Set of available cores
skl::cpu_indices_t g_skl_core_cpu_indices{};

//Is the skl core initialized
SKL_CACHE_ALIGNED std::relaxed_value<bool> g_is_skl_core_init{false};
} // namespace

namespace skl {
skl_status skl_core_init_thread__rand() noexcept;
skl_status skl_core_deinit_thread__rand() noexcept;

void skl_core_deinit_thread__slog() noexcept;
void skl_core_deinit_thread__slog_bend() noexcept;
} // namespace skl

namespace skl {
skl_status skl_core_init() noexcept {
    if (g_is_skl_core_init.exchange(true)) {
        return SKL_OK_REDUNDANT;
    }

    const auto result = SKLThread::get_process_usable_cores(g_skl_core_cpu_indices.data(), g_skl_core_cpu_indices.capacity());
    SKL_ASSERT_PERMANENT(result.is_success() && (result.value() > 0U));
    auto& tmp = g_skl_core_cpu_indices.upgrade();
    tmp.grow(result.value());

    SKL_ASSERT_PERMANENT(init_program_epilog().is_success());

    if (skl_core_init_thread().is_failure()) {
        (void)g_is_skl_core_init.exchange(false);
        return SKL_ERR_FAIL;
    }

#if 0
    puts("SKL_CORE_INIT!");
#endif
    return SKL_SUCCESS;
}

skl_status skl_core_init_thread() noexcept {
    if (g_is_skl_core_init_on_thread) {
        return SKL_OK_REDUNDANT;
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

    skl_core_deinit_thread__slog_bend();
    skl_core_deinit_thread__slog();

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

const cpu_indices_t& skl_core_get_available_cpus() noexcept {
    return g_skl_core_cpu_indices;
}
} // namespace skl
