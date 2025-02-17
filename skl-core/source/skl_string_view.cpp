//!
//! \file skl_string_view
//!
//! \brief Linux x86-x64 clang 19+ - minimal string view
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <string_view>

#include "skl_string_view"

namespace skl {
template <>
constexpr skl_string_view::skl_string_view(const std::string_view& view) noexcept
    : m_len(view.length())
    , m_str(view.data()) {
}
} // namespace skl
