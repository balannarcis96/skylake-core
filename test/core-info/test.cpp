#include <skl_core>
#include <skl_core_info>

#include <gtest/gtest.h>

TEST(SkylakeCoreInfo, General) {
    ASSERT_TRUE(skl::skl_core_get_available_cpus().empty());
    ASSERT_TRUE(skl::skl_core_init().is_success());
    ASSERT_FALSE(skl::skl_core_get_available_cpus().empty());

    printf("Available core indices:\n\t");
    for (auto cpu_index : skl::skl_core_get_available_cpus()) {
        printf("%u ", u32(cpu_index));
    }
    puts("");
}
