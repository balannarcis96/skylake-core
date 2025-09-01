#include <skl_static_bitset>

#include <gtest/gtest.h>

using fixed_static_bitset_1_t = skl::StaticBitSet<7u, true, false>;
using fixed_static_bitset_2_t = skl::StaticBitSet<15u, true, false>;
using fixed_static_bitset_3_t = skl::StaticBitSet<31u, true, false>;
using fixed_static_bitset_4_t = skl::StaticBitSet<63u, true, false>;
using fixed_static_bitset_5_t = skl::StaticBitSet<255u, true, false>;

using fixed_static_bitset_1_count_t = skl::StaticBitSet<7u, true, true>;
using fixed_static_bitset_2_count_t = skl::StaticBitSet<15u, true, true>;
using fixed_static_bitset_3_count_t = skl::StaticBitSet<31u, true, true>;
using fixed_static_bitset_4_count_t = skl::StaticBitSet<63u, true, true>;
using fixed_static_bitset_5_count_t = skl::StaticBitSet<255u, true, true>;

TEST(SkylakeStaticBitset, basic_set) {
    using test_bitset_t = skl::StaticBitSet<4u>;
    static_assert(test_bitset_t::CSize == 4u);
    static_assert(test_bitset_t::CBitSize == 8u);
    static_assert(test_bitset_t::CBitSizeOfSlice == 8u);
    static_assert(test_bitset_t::CSliceCount == 1u);
    static_assert(test_bitset_t::CLeftOverBits == 4u);
    static_assert(test_bitset_t::CLeftOverBitsMask == u8(u8(u8(1u << 4u) - 1u) << 4u));
    static_assert(test_bitset_t::CBitSizeOfSlice - test_bitset_t::CLeftOverBits == 4u);

    test_bitset_t bitset{};
    ASSERT_EQ(0u, bitset.count());

    ASSERT_FALSE(bitset.set(0u));
    ASSERT_TRUE(bitset.set(0u));
    ASSERT_EQ(1u, bitset.count());

    ASSERT_FALSE(bitset.set(1u));
    ASSERT_TRUE(bitset.set(1u));
    ASSERT_EQ(2u, bitset.count());

    ASSERT_FALSE(bitset.set(2u));
    ASSERT_TRUE(bitset.set(2u));
    ASSERT_EQ(3u, bitset.count());

    ASSERT_FALSE(bitset.set(3u));
    ASSERT_TRUE(bitset.set(3u));
    ASSERT_EQ(4u, bitset.count());

    ASSERT_TRUE(bitset.test(0u));
    ASSERT_TRUE(bitset.test(1u));
    ASSERT_TRUE(bitset.test(2u));
    ASSERT_TRUE(bitset.test(3u));
    ASSERT_DEATH((void)bitset.test(4u), ".*");

    ASSERT_DEATH((void)bitset.set(4u), ".*");

    ASSERT_TRUE(bitset.unset(0u));
    ASSERT_FALSE(bitset.unset(0u));
    ASSERT_EQ(3u, bitset.count());

    ASSERT_TRUE(bitset.unset(1u));
    ASSERT_FALSE(bitset.unset(1u));
    ASSERT_EQ(2u, bitset.count());

    ASSERT_TRUE(bitset.unset(2u));
    ASSERT_FALSE(bitset.unset(2u));
    ASSERT_EQ(1u, bitset.count());

    ASSERT_TRUE(bitset.unset(3u));
    ASSERT_FALSE(bitset.unset(3u));
    ASSERT_EQ(0u, bitset.count());

    ASSERT_DEATH((void)bitset.unset(4u), ".*");

    ASSERT_FALSE(bitset.test(0u));
    ASSERT_FALSE(bitset.test(1u));
    ASSERT_FALSE(bitset.test(2u));
    ASSERT_FALSE(bitset.test(3u));
    ASSERT_DEATH((void)bitset.test(4u), ".*");

    ASSERT_FALSE(bitset.set(0u));
    ASSERT_FALSE(bitset.set(1u));
    ASSERT_FALSE(bitset.set(2u));
    ASSERT_FALSE(bitset.set(3u));
    ASSERT_EQ(4u, bitset.count());
    bitset.reset();
    ASSERT_FALSE(bitset.test(0u));
    ASSERT_FALSE(bitset.test(1u));
    ASSERT_FALSE(bitset.test(2u));
    ASSERT_FALSE(bitset.test(3u));
    ASSERT_EQ(0u, bitset.count());
}

TEST(SkylakeStaticBitset, basic_set_passive_count) {
    using test_bitset_t = skl::StaticBitSet<4u, false, true>;
    static_assert(test_bitset_t::CSize == 4u);
    static_assert(test_bitset_t::CBitSize == 8u);
    static_assert(test_bitset_t::CBitSizeOfSlice == 8u);
    static_assert(test_bitset_t::CSliceCount == 1u);
    static_assert(test_bitset_t::CLeftOverBits == 4u);
    static_assert(test_bitset_t::CLeftOverBitsMask == u8(u8(u8(1u << 4u) - 1u) << 4u));
    static_assert(test_bitset_t::CBitSizeOfSlice - test_bitset_t::CLeftOverBits == 4u);

    test_bitset_t bitset{};
    ASSERT_EQ(0u, bitset.count());

    ASSERT_FALSE(bitset.set(0u));
    ASSERT_TRUE(bitset.set(0u));
    ASSERT_EQ(1u, bitset.count());

    ASSERT_FALSE(bitset.set(1u));
    ASSERT_TRUE(bitset.set(1u));
    ASSERT_EQ(2u, bitset.count());

    ASSERT_FALSE(bitset.set(2u));
    ASSERT_TRUE(bitset.set(2u));
    ASSERT_EQ(3u, bitset.count());

    ASSERT_FALSE(bitset.set(3u));
    ASSERT_TRUE(bitset.set(3u));
    ASSERT_EQ(4u, bitset.count());

    ASSERT_TRUE(bitset.test(0u));
    ASSERT_TRUE(bitset.test(1u));
    ASSERT_TRUE(bitset.test(2u));
    ASSERT_TRUE(bitset.test(3u));
    ASSERT_DEATH((void)bitset.test(4u), ".*");

    ASSERT_DEATH((void)bitset.set(4u), ".*");

    ASSERT_TRUE(bitset.unset(0u));
    ASSERT_FALSE(bitset.unset(0u));
    ASSERT_EQ(3u, bitset.count());

    ASSERT_TRUE(bitset.unset(1u));
    ASSERT_FALSE(bitset.unset(1u));
    ASSERT_EQ(2u, bitset.count());

    ASSERT_TRUE(bitset.unset(2u));
    ASSERT_FALSE(bitset.unset(2u));
    ASSERT_EQ(1u, bitset.count());

    ASSERT_TRUE(bitset.unset(3u));
    ASSERT_FALSE(bitset.unset(3u));
    ASSERT_EQ(0u, bitset.count());

    ASSERT_DEATH((void)bitset.unset(4u), ".*");

    ASSERT_FALSE(bitset.test(0u));
    ASSERT_FALSE(bitset.test(1u));
    ASSERT_FALSE(bitset.test(2u));
    ASSERT_FALSE(bitset.test(3u));
    ASSERT_DEATH((void)bitset.test(4u), ".*");
}

TEST(SkylakeStaticBitset, basic_reset_passive_count_default_true) {
    using test_bitset_t = skl::StaticBitSet<4u, true, false>;

    test_bitset_t bitset{};

    ASSERT_EQ(4u, bitset.count());
    ASSERT_TRUE(bitset.unset(0u));
    ASSERT_TRUE(bitset.unset(1u));
    ASSERT_TRUE(bitset.unset(2u));
    ASSERT_TRUE(bitset.unset(3u));
    ASSERT_EQ(0u, bitset.count());
    bitset.reset();
    ASSERT_EQ(4u, bitset.count());
}

TEST(SkylakeStaticBitset, basic_reset_active_count_default_true) {
    using test_bitset_t = skl::StaticBitSet<4u, true, true>;

    test_bitset_t bitset{};

    ASSERT_EQ(4u, bitset.count());
    ASSERT_TRUE(bitset.unset(0u));
    ASSERT_TRUE(bitset.unset(1u));
    ASSERT_TRUE(bitset.unset(2u));
    ASSERT_TRUE(bitset.unset(3u));
    ASSERT_EQ(0u, bitset.count());
    bitset.reset();
    ASSERT_EQ(4u, bitset.count());
}

TEST(SkylakeStaticBitset, basic_set_all_active_count_default_true) {
    using test_bitset_t = skl::StaticBitSet<4u, true, true>;

    test_bitset_t bitset{};

    ASSERT_EQ(4u, bitset.count());
    bitset.set_all_to(false);
    ASSERT_EQ(0u, bitset.count());
    bitset.reset();
    ASSERT_EQ(4u, bitset.count());
    bitset.set_all_to<false>();
    ASSERT_EQ(0u, bitset.count());
}

TEST(SkylakeStaticBitset, basic_set_all_passive_count_default_true) {
    using test_bitset_t = skl::StaticBitSet<4u, true, false>;

    test_bitset_t bitset{};

    ASSERT_EQ(4u, bitset.count());
    bitset.set_all_to(false);
    ASSERT_EQ(0u, bitset.count());
    bitset.reset();
    ASSERT_EQ(4u, bitset.count());
    bitset.set_all_to<false>();
    ASSERT_EQ(0u, bitset.count());
}

TEST(SkylakeStaticBitset, basic_flip) {
    {
        using test_bitset_t = skl::StaticBitSet<4u, true, false>;

        test_bitset_t bitset{};

        ASSERT_EQ(4u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(0u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(4u, bitset.count());
    }
    {
        using test_bitset_t = skl::StaticBitSet<4u, true, true>;

        test_bitset_t bitset{};

        ASSERT_EQ(4u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(0u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(4u, bitset.count());
    }
    {
        using test_bitset_t = skl::StaticBitSet<4u, false, false>;

        test_bitset_t bitset{};

        ASSERT_EQ(0u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(4u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(0u, bitset.count());
    }
    {
        using test_bitset_t = skl::StaticBitSet<4u, false, true>;

        test_bitset_t bitset{};

        ASSERT_EQ(0u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(4u, bitset.count());
        bitset.flip_all();
        ASSERT_EQ(0u, bitset.count());
    }
}

TEST(SkylakeStaticBitset, basic) {
    using test_pred_t = decltype([](auto& f_bitset) static noexcept {
        using bitset_t     = std::decay_t<decltype(f_bitset)>;
        using alt_bitset_t = skl::StaticBitSet<bitset_t::CSize>;

        ASSERT_EQ(f_bitset.count(), bitset_t::CSize);

        alt_bitset_t alt_bitset{};
        ASSERT_TRUE(alt_bitset.count() == 0u);
    });

    fixed_static_bitset_1_t fixed_static_bitset_1{};
    test_pred_t::           operator()(fixed_static_bitset_1);
    fixed_static_bitset_2_t fixed_static_bitset_2{};
    test_pred_t::           operator()(fixed_static_bitset_2);
    fixed_static_bitset_3_t fixed_static_bitset_3{};
    test_pred_t::           operator()(fixed_static_bitset_3);
    fixed_static_bitset_4_t fixed_static_bitset_4{};
    test_pred_t::           operator()(fixed_static_bitset_4);
    fixed_static_bitset_5_t fixed_static_bitset_5{};
    test_pred_t::           operator()(fixed_static_bitset_5);
}

TEST(SkylakeStaticBitset, basic_count) {
    using test_pred_t = decltype([](auto& f_bitset) static noexcept {
        using bitset_t     = std::decay_t<decltype(f_bitset)>;
        using alt_bitset_t = skl::StaticBitSet<bitset_t::CSize, false, true>;

        ASSERT_TRUE(f_bitset.count() == bitset_t::CSize);

        alt_bitset_t alt_bitset{};
        ASSERT_TRUE(alt_bitset.count() == 0u);
    });

    fixed_static_bitset_1_count_t fixed_static_bitset_1{};
    test_pred_t::                 operator()(fixed_static_bitset_1);
    fixed_static_bitset_2_count_t fixed_static_bitset_2{};
    test_pred_t::                 operator()(fixed_static_bitset_2);
    fixed_static_bitset_3_count_t fixed_static_bitset_3{};
    test_pred_t::                 operator()(fixed_static_bitset_3);
    fixed_static_bitset_4_count_t fixed_static_bitset_4{};
    test_pred_t::                 operator()(fixed_static_bitset_4);
    fixed_static_bitset_5_count_t fixed_static_bitset_5{};
    test_pred_t::                 operator()(fixed_static_bitset_5);
}

static_assert(sizeof(fixed_static_bitset_1_t) == 1u);
static_assert(sizeof(fixed_static_bitset_2_t) == 2u);
static_assert(sizeof(fixed_static_bitset_3_t) == 4u);
static_assert(sizeof(fixed_static_bitset_4_t) == 8u);
static_assert(sizeof(fixed_static_bitset_5_t) == 32u);

static_assert(fixed_static_bitset_1_t::CSize == 7u);
static_assert(fixed_static_bitset_2_t::CSize == 15u);
static_assert(fixed_static_bitset_3_t::CSize == 31u);
static_assert(fixed_static_bitset_4_t::CSize == 63u);
static_assert(fixed_static_bitset_5_t::CSize == 255u);

static_assert(fixed_static_bitset_1_t::CBitSize == 8u);
static_assert(fixed_static_bitset_2_t::CBitSize == 16u);
static_assert(fixed_static_bitset_3_t::CBitSize == 32u);
static_assert(fixed_static_bitset_4_t::CBitSize == 64u);
static_assert(fixed_static_bitset_5_t::CBitSize == 256u);

static_assert(sizeof(fixed_static_bitset_1_count_t) == 2u);
static_assert(sizeof(fixed_static_bitset_2_count_t) == 4u);
static_assert(sizeof(fixed_static_bitset_3_count_t) == 8u);
static_assert(sizeof(fixed_static_bitset_4_count_t) == 16u);
static_assert(sizeof(fixed_static_bitset_5_count_t) == 40u);

static_assert(fixed_static_bitset_1_t::CLeftOverBits == 1u);
static_assert(fixed_static_bitset_1_t::CLeftOverBitsMask == u8(128u));
