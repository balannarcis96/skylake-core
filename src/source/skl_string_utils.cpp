//!
//! \file skl_string_utils
//!
//! \brief Linux x86-x64 clang 19+ - minimal thread local string utils
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <algorithm>
#include <string>

namespace skl {

void left_trim(std::string& f_string, const char f_trim_char) noexcept {
    const auto it = std::find_if(f_string.begin(), f_string.end(), [f_trim_char](char f_ch) noexcept {
        return f_ch != f_trim_char;
    });
    
    f_string.erase(f_string.begin(), it);
}

void right_trim(std::string& f_string, const char f_trim_char) noexcept {
    const auto it = std::find_if(f_string.rbegin(), f_string.rend(), [f_trim_char](char f_ch) noexcept {
        return f_ch != f_trim_char;
    });

    f_string.erase(it.base(), f_string.end());
}

} // namespace skl
