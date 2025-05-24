//!
//! \file skl_status
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstring>

#include "skl_status"
#include "skl_magic_enum"

namespace {
thread_local char g_skl_core_invalid_status_enum[1024U];

SKL_NOINLINE const char* skl_core_on_invalid_skl_status_enum(skl_status_raw f_status) noexcept {
    const auto result = snprintf(g_skl_core_invalid_status_enum, std::size(g_skl_core_invalid_status_enum) - 1U, "[No enum entry for skl_status=%d]", f_status);
    if (0 >= result) {
        (void)strcpy(g_skl_core_invalid_status_enum, "[skl_status::to_string() ERROR!]");
    } else {
        g_skl_core_invalid_status_enum[result] = 0;
    }
    return g_skl_core_invalid_status_enum;
}
} // namespace

const char* skl_status::to_string() const noexcept {
    const auto view = skl::enum_to_string(raw_as_enum<ESklCoreStatus>());
    if (view.empty()) {
        return skl_core_on_invalid_skl_status_enum(raw());
    }

    //We are guaranted that this is a view to a c-string
    [[likely]] return view.data();
}
