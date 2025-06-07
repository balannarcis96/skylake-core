//!
//! \file skl_resources_dir
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <filesystem>
#include <string>
#include <cstring>

#include "skl_resources_dir"

namespace {
thread_local char g_skl_core_res_dir_work_buffer[skl::CPathMaxLength];
}

namespace skl {
skl_status ResourcesDirectory::set_base_path(skl_string_view f_path) noexcept {
    if (false == f_path.empty()) {
        auto required_length = f_path.length();
        if (false == f_path.std<std::string_view>().ends_with('/')) {
            ++required_length;
        }

        if (required_length >= std::size(m_path)) {
            return SKL_ERR_PARAMS;
        }

        f_path.copy_and_terminate(m_path);

        if (false == f_path.std<std::string_view>().ends_with('/')) {
            m_path[f_path.length()]      = '/';
            m_path[f_path.length() + 1U] = 0;
        }
    } else {
        m_path[0U] = '.';
        m_path[1U] = '/';
        m_path[2U] = 0;
    }

    return SKL_SUCCESS;
}

skl_string_view ResourcesDirectory::base_path() const noexcept {
    return skl_string_view::from_cstr(m_path);
}

skl_string_view ResourcesDirectory::base_path_absolute() const noexcept {
    const auto path = std::filesystem::absolute(m_path);
    (void)strncpy(g_skl_core_res_dir_work_buffer, path.c_str(), std::size(g_skl_core_res_dir_work_buffer) - 1U);
    g_skl_core_res_dir_work_buffer[std::size(g_skl_core_res_dir_work_buffer) - 1U] = 0;
    return skl_string_view::from_cstr(g_skl_core_res_dir_work_buffer);
}

skl_result<skl_string_view> ResourcesDirectory::make_path(skl_string_view f_sub_path) const noexcept {
    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    if (str.length() > (std::size(g_skl_core_res_dir_work_buffer) - 1U)) {
        return skl_fail{SKL_ERR_SIZE};
    }
    const auto length                      = str.copy(g_skl_core_res_dir_work_buffer, std::size(g_skl_core_res_dir_work_buffer) - 1U);
    g_skl_core_res_dir_work_buffer[length] = 0;
    return skl_string_view::from_cstr(g_skl_core_res_dir_work_buffer);
}

skl_result<skl_string_view> ResourcesDirectory::make_path_absolute(skl_string_view f_sub_path) const noexcept {
    auto path = std::filesystem::absolute(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    if (str.length() > (std::size(g_skl_core_res_dir_work_buffer) - 1U)) {
        return skl_fail{SKL_ERR_SIZE};
    }
    const auto length                      = str.copy(g_skl_core_res_dir_work_buffer, std::size(g_skl_core_res_dir_work_buffer) - 1U);
    g_skl_core_res_dir_work_buffer[length] = 0;
    return skl_string_view::from_cstr(g_skl_core_res_dir_work_buffer);
}

skl_result<skl_string_view> ResourcesDirectory::make_path(skl_string_view f_sub_path, skl_buffer_view f_target_buffer) const noexcept {
    if (f_target_buffer.length < 2U) {
        return skl_fail{SKL_ERR_PARAMS};
    }

    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    if (str.length() > (f_target_buffer.length - 1U)) {
        return skl_fail{SKL_ERR_SIZE};
    }
    const auto length              = str.copy(reinterpret_cast<char*>(f_target_buffer.buffer), f_target_buffer.length - 1U);
    f_target_buffer.buffer[length] = 0;
    f_target_buffer.position       = length;

    return skl_string_view::exact(reinterpret_cast<const char*>(f_target_buffer.buffer), f_target_buffer.position);
}

skl_string_view ResourcesDirectory::make_path_checked(skl_string_view f_sub_path) const noexcept {
    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    SKL_ASSERT_PERMANENT(str.length() <= (std::size(g_skl_core_res_dir_work_buffer) - 1U));
    const auto length                      = str.copy(g_skl_core_res_dir_work_buffer, std::size(g_skl_core_res_dir_work_buffer) - 1U);
    g_skl_core_res_dir_work_buffer[length] = 0;
    return skl_string_view::from_cstr(g_skl_core_res_dir_work_buffer);
}

skl_string_view ResourcesDirectory::make_path_checked(skl_string_view f_sub_path, skl_buffer_view f_target_buffer) const noexcept {
    SKL_ASSERT_PERMANENT(f_target_buffer.length >= 2U);
    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    SKL_ASSERT_PERMANENT(str.length() <= (f_target_buffer.length - 1U));
    const auto length              = str.copy(reinterpret_cast<char*>(f_target_buffer.buffer), f_target_buffer.length - 1U);
    f_target_buffer.buffer[length] = 0;
    f_target_buffer.position       = length;
    return skl_string_view::exact(reinterpret_cast<const char*>(f_target_buffer.buffer), f_target_buffer.position);
}

skl_string_view ResourcesDirectory::make_path_absolute_checked(skl_string_view f_sub_path) const noexcept {
    auto path = std::filesystem::absolute(m_path) / f_sub_path.std<std::string_view>();
    auto str  = path.string();
    SKL_ASSERT_PERMANENT(str.length() <= (std::size(g_skl_core_res_dir_work_buffer) - 1U));
    const auto length                      = str.copy(g_skl_core_res_dir_work_buffer, std::size(g_skl_core_res_dir_work_buffer) - 1U);
    g_skl_core_res_dir_work_buffer[length] = 0;
    return skl_string_view::from_cstr(g_skl_core_res_dir_work_buffer);
}

bool ResourcesDirectory::path_exists(skl_string_view f_sub_path) noexcept {
    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    return std::filesystem::exists(path);
}

skl_status ResourcesDirectory::create_directory(skl_string_view f_sub_path, bool f_delete_if_exists) const noexcept {
    auto path = std::filesystem::path(m_path) / f_sub_path.std<std::string_view>();
    if (std::filesystem::is_directory(path)) {
        if (false == f_delete_if_exists) {
            return SKL_ERR_STATE;
        }

        if (0U == std::filesystem::remove_all(path)) {
            return SKL_ERR_FAIL;
        }
    }

    if (false == std::filesystem::create_directories(path)) {
        return SKL_ERR_FAIL;
    }

    return SKL_SUCCESS;
}
} // namespace skl
