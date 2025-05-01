//!
//! \file skl_memory
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!

#include <cstring>

#include "skl_int"

namespace skl {

u32 skl_strlen_unsafe(const char* f_str) noexcept {
    return strlen(f_str);
}

u32 skl_strlen(const char* f_str, u32 f_max_len) noexcept {
    return u32(strnlen(f_str, f_max_len));
}
} // namespace skl
