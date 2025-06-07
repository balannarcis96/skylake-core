#include <iostream>
#include <filesystem>

#include <skl_resources_dir>

#include <gtest/gtest.h>

TEST(ResourcesDirectory, General) {
    if (std::filesystem::exists("./resources")) {
        std::filesystem::remove_all("./resources");
    }
    ASSERT_TRUE(std::filesystem::create_directory("./resources"));

    skl::ResourcesDirectory res_dir{};

    ASSERT_TRUE(res_dir.set_base_path(skl::skl_string_view::from_cstr("./resources")).to_bool());

    const auto sub_path_result = res_dir.make_path(skl::skl_string_view::exact_cstr("sub_path"));
    ASSERT_TRUE(sub_path_result.is_success());

    const auto sub_path = sub_path_result.value().std<std::string_view>();
    std::cout << sub_path << "\n";

    ASSERT_FALSE(std::filesystem::exists(sub_path));
    ASSERT_TRUE(res_dir.create_directory(skl::skl_string_view::exact_cstr("sub_path")).to_bool());
    ASSERT_TRUE(std::filesystem::exists(sub_path));
    ASSERT_TRUE(0U != std::filesystem::remove_all(sub_path));

    ASSERT_FALSE(std::filesystem::exists(sub_path));
    ASSERT_TRUE(std::filesystem::create_directories(std::filesystem::path(sub_path) / "sub_sub_path"));
    ASSERT_EQ(res_dir.create_directory(skl::skl_string_view::exact_cstr("sub_path")), SKL_ERR_STATE);
    ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(sub_path) / "sub_sub_path"));

    ASSERT_TRUE(res_dir.create_directory(skl::skl_string_view::exact_cstr("sub_path"), true).to_bool());
    ASSERT_TRUE(std::filesystem::exists(sub_path));
    ASSERT_TRUE(std::filesystem::is_empty(sub_path));

    ASSERT_TRUE(0U != std::filesystem::remove_all(res_dir.base_path().std<std::string_view>()));
}
