#include <skl_flags>

#include <gtest/gtest.h>

using flags_api_u32 = skl::flags_api<u32>;

// Test with enum type
enum class MyFlags : u32 {
    None  = 0,
    Flag1 = 1 << 0,
    Flag2 = 1 << 1,
    Flag3 = 1 << 2,
    Flag4 = 1 << 3,
    All   = Flag1 | Flag2 | Flag3 | Flag4
};

using flags_api_enum = skl::flags_api<u32, MyFlags>;

TEST(SklFlags, basic_set_clear) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    ASSERT_TRUE(flags.empty());
    ASSERT_EQ(0u, flags.raw());

    flags.set(1u);
    ASSERT_FALSE(flags.empty());
    ASSERT_TRUE(flags.test(1u));
    ASSERT_EQ(1u, flags.raw());
    ASSERT_EQ(1u, value);

    flags.set(2u);
    ASSERT_TRUE(flags.test(1u));
    ASSERT_TRUE(flags.test(2u));
    ASSERT_TRUE(flags.test(3u)); // 1 | 2 = 3
    ASSERT_EQ(3u, flags.raw());

    flags.clear(1u);
    ASSERT_FALSE(flags.test(1u));
    ASSERT_TRUE(flags.test(2u));
    ASSERT_EQ(2u, flags.raw());

    flags.reset();
    ASSERT_TRUE(flags.empty());
    ASSERT_EQ(0u, flags.raw());
}

TEST(SklFlags, template_set_clear) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    flags.set<1u, 2u, 4u>();
    ASSERT_EQ(7u, flags.raw());
    ASSERT_TRUE((flags.test<1u, 2u, 4u>()));

    flags.clear<1u, 4u>();
    ASSERT_EQ(2u, flags.raw());
    ASSERT_FALSE((flags.test<1u>()));
    ASSERT_TRUE((flags.test<2u>()));
}

TEST(SklFlags, toggle) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    flags.toggle(1u);
    ASSERT_TRUE(flags.test(1u));

    flags.toggle(1u);
    ASSERT_FALSE(flags.test(1u));

    flags.toggle<1u, 2u>();
    ASSERT_TRUE((flags.test<1u, 2u>()));

    flags.toggle<1u>();
    ASSERT_FALSE(flags.test(1u));
    ASSERT_TRUE(flags.test(2u));
}

TEST(SklFlags, any_none) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);
    flags.set(1u | 4u);

    ASSERT_TRUE(flags.any(1u));
    ASSERT_TRUE(flags.any(4u));
    ASSERT_TRUE(flags.any(1u | 2u)); // any of 1 or 2
    ASSERT_FALSE(flags.any(2u));

    ASSERT_TRUE(flags.none(2u));
    ASSERT_FALSE(flags.none(1u));
    ASSERT_FALSE(flags.none(1u | 2u)); // 1 is set

    ASSERT_TRUE((flags.any<1u, 2u>()));
    ASSERT_TRUE((flags.none<2u, 8u>()));
}

TEST(SklFlags, only) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);
    flags.set(1u | 2u);

    ASSERT_TRUE(flags.only(1u | 2u));
    ASSERT_FALSE(flags.only(1u));
    ASSERT_FALSE(flags.only(1u | 2u | 4u));

    ASSERT_TRUE((flags.only<1u, 2u>()));
    ASSERT_FALSE((flags.only<1u>()));
}

TEST(SklFlags, count) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    ASSERT_EQ(0, flags.count());

    flags.set(1u);
    ASSERT_EQ(1, flags.count());

    flags.set(2u);
    ASSERT_EQ(2, flags.count());

    flags.set(4u);
    ASSERT_EQ(3, flags.count());

    flags.set(0xFFu);
    ASSERT_EQ(8, flags.count());
}

TEST(SklFlags, assign) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    flags.assign(0xABCDu);
    ASSERT_EQ(0xABCDu, flags.raw());
    ASSERT_EQ(0xABCDu, value);

    flags.assign(0u);
    ASSERT_TRUE(flags.empty());
}

TEST(SklFlags, set_if_clear_if) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    flags.set_if(1u, true);
    ASSERT_TRUE(flags.test(1u));

    flags.set_if(2u, false);
    ASSERT_FALSE(flags.test(2u));

    flags.clear_if(1u, false);
    ASSERT_TRUE(flags.test(1u));

    flags.clear_if(1u, true);
    ASSERT_FALSE(flags.test(1u));
}

TEST(SklFlags, chaining) {
    u32 value = 0;
    auto& flags = flags_api_u32::make(value);

    flags.set(1u).set(2u).toggle(4u).clear(1u);
    ASSERT_EQ(2u | 4u, flags.raw());

    flags.reset().set<1u, 2u, 4u>().clear<2u>();
    ASSERT_EQ(1u | 4u, flags.raw());
}

TEST(SklFlags, enum_basic) {
    u32 value = 0;
    auto& flags = flags_api_enum::make(value);

    ASSERT_TRUE(flags.empty());

    flags.set(MyFlags::Flag1);
    ASSERT_TRUE(flags.test(MyFlags::Flag1));
    ASSERT_FALSE(flags.test(MyFlags::Flag2));

    flags.set(MyFlags::Flag2);
    ASSERT_TRUE(flags.any(MyFlags::Flag1));
    ASSERT_TRUE(flags.any(MyFlags::Flag2));

    flags.clear(MyFlags::Flag1);
    ASSERT_FALSE(flags.test(MyFlags::Flag1));
    ASSERT_TRUE(flags.test(MyFlags::Flag2));

    flags.toggle(MyFlags::Flag3);
    ASSERT_TRUE(flags.test(MyFlags::Flag3));
}

TEST(SklFlags, enum_conditional) {
    u32 value = 0;
    auto& flags = flags_api_enum::make(value);

    flags.set_if(MyFlags::Flag1, true);
    ASSERT_TRUE(flags.test(MyFlags::Flag1));

    flags.set_if(MyFlags::Flag2, false);
    ASSERT_FALSE(flags.test(MyFlags::Flag2));

    flags.clear_if(MyFlags::Flag1, true);
    ASSERT_FALSE(flags.test(MyFlags::Flag1));
}

TEST(SklFlags, enum_only) {
    u32 value = 0;
    auto& flags = flags_api_enum::make(value);

    flags.assign(MyFlags::Flag1);
    ASSERT_TRUE(flags.only(MyFlags::Flag1));

    flags.set(MyFlags::Flag2);
    ASSERT_FALSE(flags.only(MyFlags::Flag1));
}

// Static assertion: flags_api has no data members (zero-size for type-punning)
static_assert(sizeof(flags_api_u32) == 1); // Empty class has size 1 in C++
