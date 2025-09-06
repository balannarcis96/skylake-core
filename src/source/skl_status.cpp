//!
//! \file skl_status
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstring>

#include "skl_atomic"
#include "skl_status"
#include "skl_magic_enum"

namespace {
std::synched_value<skl_status::to_string_fn_t*> g_skl_custom_to_string_handler = nullptr;
thread_local char                               g_skl_core_invalid_status_enum[1024U];

SKL_NOINLINE const char* skl_core_on_invalid_skl_status_enum(skl_status_raw f_status) noexcept {
    const auto result = snprintf(g_skl_core_invalid_status_enum, std::size(g_skl_core_invalid_status_enum) - 1U, "[No enum entry for skl_status=%d]", f_status);
    if (0 >= result) {
        (void)strcpy(g_skl_core_invalid_status_enum, "[skl_status::to_string() ERROR!]");
    } else {
        g_skl_core_invalid_status_enum[result] = 0;
    }
    return g_skl_core_invalid_status_enum;
}

SKL_NOINLINE const char* skl_core_on_invalid_custom_skl_status_enum(skl_status_raw f_status) noexcept {
    const auto result = snprintf(g_skl_core_invalid_status_enum, std::size(g_skl_core_invalid_status_enum) - 1U, "[No enum entry for skl_status=%d] (Custom)", f_status);
    if (0 >= result) {
        (void)strcpy(g_skl_core_invalid_status_enum, "[skl_status::to_string() ERROR!]");
    } else {
        g_skl_core_invalid_status_enum[result] = 0;
    }
    return g_skl_core_invalid_status_enum;
}
} // namespace

const char* skl_status::to_string() const noexcept {
    if (is_custom()) {
        const auto handler = g_skl_custom_to_string_handler.load();
        if (handler != nullptr) {
            return handler(m_status);
        }

        return skl_core_on_invalid_custom_skl_status_enum(raw());
    }

    const auto view = skl::enum_to_string(raw_as_enum<ESklCoreStatus>());
    if (view.empty()) {
        return skl_core_on_invalid_skl_status_enum(raw());
    }

    //We are guaranted that this is a view to a c-string
    [[likely]] return view.data();
}

void skl_status::register_custom_to_string_handler(to_string_fn_t* f_function_ptr) noexcept {
    g_skl_custom_to_string_handler = f_function_ptr;
}
