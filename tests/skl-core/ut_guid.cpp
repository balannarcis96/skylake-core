#include <gtest/gtest.h>

#include <skl_guid>
#include <skl_sguid>
#include <skl_core>

static_assert(sizeof(skl::SGUID) == sizeof(u32));
static_assert(sizeof(skl::GUID) == (sizeof(u64) * 2U));

TEST(SKLCoreGuidTests, guid_basics) {
    skl::GUID  guid{};
    skl::SGUID sguid{};

    ASSERT_TRUE(guid.is_null());
    ASSERT_TRUE(sguid.is_null());
}

TEST(SKLCoreGuidTests, guid_create_global) {
    skl::GUID  guid  = skl::g_make_guid();
    skl::SGUID sguid = skl::g_make_sguid();

    ASSERT_FALSE(guid.is_null());
    ASSERT_FALSE(sguid.is_null());
}

TEST(SKLCoreGuidTests, guid_create) {
    ASSERT_EQ(skl::skl_core_init(), SKL_SUCCESS);

    skl::GUID  guid  = skl::make_guid();
    skl::SGUID sguid = skl::make_sguid();

    ASSERT_FALSE(guid.is_null());
    ASSERT_FALSE(sguid.is_null());

    byte temp[128U]{0};
    guid.to_string(temp);
    puts(reinterpret_cast<const char*>(temp));
    sguid.to_string(temp);
    puts(reinterpret_cast<const char*>(temp));

    ASSERT_EQ(skl::skl_core_deinit(), SKL_SUCCESS);
}
