#include <skl_dynamic_bitset>

#include <gtest/gtest.h>

using bitset_1_t = skl::DynamicBitSet<true, true>;
using bitset_2_t = skl::DynamicBitSet<true, false>;
using bitset_3_t = skl::DynamicBitSet<false, true>;
using bitset_4_t = skl::DynamicBitSet<false, false>;

static_assert(bitset_1_t::CIsFixedSize == false);
static_assert(bitset_1_t::CBitSizeOfSlice == 64u);
static_assert(bitset_1_t::CDefaultBitValue == true);
static_assert(bitset_1_t::CDefaultSliceValue == ~u64(0u));

static_assert(bitset_2_t::CIsFixedSize == false);
static_assert(bitset_2_t::CBitSizeOfSlice == 64u);
static_assert(bitset_2_t::CDefaultBitValue == true);
static_assert(bitset_2_t::CDefaultSliceValue == ~u64(0u));

static_assert(bitset_3_t::CIsFixedSize == false);
static_assert(bitset_3_t::CBitSizeOfSlice == 64u);
static_assert(bitset_3_t::CDefaultBitValue == false);
static_assert(bitset_3_t::CDefaultSliceValue == u64(0u));

static_assert(bitset_4_t::CIsFixedSize == false);
static_assert(bitset_4_t::CBitSizeOfSlice == 64u);
static_assert(bitset_4_t::CDefaultBitValue == false);
static_assert(bitset_4_t::CDefaultSliceValue == u64(0u));

TEST(SkylakeDynamicBitset, basic_count_and_size) {
    using test_pred_t = decltype([](auto& f_bitset) static noexcept {
        ASSERT_EQ(f_bitset.size(), 0u);
        ASSERT_EQ(f_bitset.count(), 0u);
        ASSERT_EQ(f_bitset.slices_count(), 0u);
        ASSERT_DEATH(f_bitset.set(0u), ".*");
        ASSERT_DEATH(f_bitset.unset(0u), ".*");
        ASSERT_DEATH((void)f_bitset.test(0u), ".*");
        ASSERT_DEATH((void)f_bitset.slice(0u), ".*");

        f_bitset.template set_all_to<true>();
        ASSERT_EQ(f_bitset.count(), 0u);
        ASSERT_EQ(f_bitset.size(), 0u);

        f_bitset.set_all_to(true);
        ASSERT_EQ(f_bitset.count(), 0u);
        ASSERT_EQ(f_bitset.size(), 0u);

        f_bitset.flip_all();
        ASSERT_EQ(f_bitset.count(), 0u);
        ASSERT_EQ(f_bitset.size(), 0u);

        f_bitset.reset();
        ASSERT_EQ(f_bitset.count(), 0u);
        ASSERT_EQ(f_bitset.size(), 0u);
    });

    bitset_1_t   bitset_1{};
    test_pred_t::operator()(bitset_1);
    bitset_2_t   bitset_2{};
    test_pred_t::operator()(bitset_2);
    bitset_3_t   bitset_3{};
    test_pred_t::operator()(bitset_3);
    bitset_4_t   bitset_4{};
    test_pred_t::operator()(bitset_4);
}

TEST(SkylakeDynamicBitset, pre_allocated_count_and_size) {
    using test_pred_t = decltype([](auto& f_bitset) static noexcept {
        using bitset_t = std::decay_t<decltype(f_bitset)>;

        ASSERT_EQ(f_bitset.slices_count(), 1u);
        ASSERT_EQ(f_bitset.size(), 4u);

        if constexpr (bitset_t::CDefaultBitValue) {
            ASSERT_EQ(f_bitset.count(), 4u);

            ASSERT_TRUE(f_bitset.set(0u));
            ASSERT_TRUE(f_bitset.set(1u));
            ASSERT_TRUE(f_bitset.set(2u));
            ASSERT_TRUE(f_bitset.set(3u));
            ASSERT_DEATH(f_bitset.set(4u), ".*");
            ASSERT_EQ(f_bitset.count(), 4u);

            ASSERT_TRUE(f_bitset.test(0u));
            ASSERT_TRUE(f_bitset.test(1u));
            ASSERT_TRUE(f_bitset.test(2u));
            ASSERT_TRUE(f_bitset.test(3u));
            ASSERT_DEATH((void)f_bitset.test(4u), ".*");

            ASSERT_TRUE(f_bitset.unset(0u));
            ASSERT_EQ(f_bitset.count(), 3u);
            ASSERT_TRUE(f_bitset.unset(1u));
            ASSERT_EQ(f_bitset.count(), 2u);
            ASSERT_TRUE(f_bitset.unset(2u));
            ASSERT_EQ(f_bitset.count(), 1u);
            ASSERT_TRUE(f_bitset.unset(3u));
            ASSERT_EQ(f_bitset.count(), 0u);

            ASSERT_DEATH(f_bitset.unset(4u), ".*");
            ASSERT_EQ(f_bitset.count(), 0u);

            f_bitset.reset();
            ASSERT_EQ(f_bitset.count(), 4u);

            ASSERT_EQ(f_bitset.slices_count(), 1u);
            ASSERT_EQ(f_bitset.size(), 4u);

            f_bitset.grow(8u);
            ASSERT_EQ(f_bitset.count(), 8u);
            ASSERT_DEATH((void)f_bitset.set(8u), ".*");
            ASSERT_DEATH((void)f_bitset.unset(8u), ".*");
            ASSERT_DEATH((void)f_bitset.test(8u), ".*");
            ASSERT_EQ(f_bitset.slices_count(), 1u);
            ASSERT_EQ(f_bitset.size(), 8u);

            for (u32 i = 0u; i < 8u; ++i) {
                ASSERT_TRUE(f_bitset.test(i));
                ASSERT_TRUE(f_bitset.unset(i));
            }
            ASSERT_EQ(f_bitset.count(), 0u);
            ASSERT_EQ(f_bitset.size(), 8u);

            f_bitset.grow(16u);
            ASSERT_EQ(f_bitset.count(), 8u);
            ASSERT_EQ(f_bitset.size(), 16u);

            ASSERT_TRUE(f_bitset.unset(10u));
            ASSERT_EQ(f_bitset.count(), 7u);

            for (u32 i = 0u; i < 8u; ++i) {
                ASSERT_FALSE(f_bitset.test(i));
            }

            for (u32 i = 8u; i < 16u; ++i) {
                if (i == 10u) {
                    ASSERT_FALSE(f_bitset.test(i));
                    continue;
                }

                ASSERT_TRUE(f_bitset.test(i));
            }

            ASSERT_EQ(f_bitset.count(), 7u);
            f_bitset.grow(50000u);
            ASSERT_EQ(f_bitset.count(), 49991u);
            ASSERT_EQ(f_bitset.size(), 50000u);
            ASSERT_EQ(f_bitset.slices_count(), skl::integral_ceil(50000u, 64u));
        } else {
            ASSERT_EQ(f_bitset.count(), 0u);

            ASSERT_FALSE(f_bitset.set(0u));
            ASSERT_EQ(f_bitset.count(), 1u);

            ASSERT_FALSE(f_bitset.set(1u));
            ASSERT_EQ(f_bitset.count(), 2u);

            ASSERT_FALSE(f_bitset.set(2u));
            ASSERT_EQ(f_bitset.count(), 3u);

            ASSERT_FALSE(f_bitset.set(3u));
            ASSERT_EQ(f_bitset.count(), 4u);

            ASSERT_DEATH(f_bitset.set(4u), ".*");
            ASSERT_EQ(f_bitset.count(), 4u);

            ASSERT_TRUE(f_bitset.test(0u));
            ASSERT_TRUE(f_bitset.test(1u));
            ASSERT_TRUE(f_bitset.test(2u));
            ASSERT_TRUE(f_bitset.test(3u));
            ASSERT_DEATH((void)f_bitset.test(4u), ".*");

            ASSERT_TRUE(f_bitset.unset(0u));
            ASSERT_EQ(f_bitset.count(), 3u);
            ASSERT_TRUE(f_bitset.unset(1u));
            ASSERT_EQ(f_bitset.count(), 2u);
            ASSERT_TRUE(f_bitset.unset(2u));
            ASSERT_EQ(f_bitset.count(), 1u);
            ASSERT_TRUE(f_bitset.unset(3u));
            ASSERT_EQ(f_bitset.count(), 0u);

            ASSERT_DEATH(f_bitset.unset(4u), ".*");
            ASSERT_EQ(f_bitset.count(), 0u);

            ASSERT_FALSE(f_bitset.set(0u));
            ASSERT_FALSE(f_bitset.set(1u));
            ASSERT_FALSE(f_bitset.set(2u));
            ASSERT_FALSE(f_bitset.set(3u));
            ASSERT_EQ(f_bitset.count(), 4u);

            f_bitset.reset();
            ASSERT_EQ(f_bitset.count(), 0u);

            ASSERT_EQ(f_bitset.slices_count(), 1u);
            ASSERT_EQ(f_bitset.size(), 4u);

            f_bitset.grow(5u);

            ASSERT_EQ(f_bitset.count(), 0u);
            ASSERT_EQ(f_bitset.slices_count(), 1u);
            ASSERT_EQ(f_bitset.size(), 5u);

            ASSERT_FALSE(f_bitset.set(0u));
            ASSERT_FALSE(f_bitset.set(1u));
            ASSERT_FALSE(f_bitset.set(2u));
            ASSERT_FALSE(f_bitset.set(3u));
            ASSERT_EQ(f_bitset.count(), 4u);
            ASSERT_FALSE(f_bitset.test(4u));
            ASSERT_DEATH((void)f_bitset.test(5u), ".*");

            f_bitset.grow(8u);
            ASSERT_EQ(f_bitset.count(), 4u);
            ASSERT_EQ(f_bitset.slices_count(), 1u);
            ASSERT_EQ(f_bitset.size(), 8u);

            ASSERT_FALSE(f_bitset.test(4u));
            ASSERT_FALSE(f_bitset.test(5u));
            ASSERT_FALSE(f_bitset.test(6u));
            ASSERT_FALSE(f_bitset.test(7u));

            ASSERT_FALSE(f_bitset.set(6u));
            ASSERT_EQ(f_bitset.count(), 5u);

            ASSERT_DEATH((void)f_bitset.test(8u), ".*");

            f_bitset.grow(50000u);
            ASSERT_EQ(f_bitset.count(), 5u);
            ASSERT_EQ(f_bitset.size(), 50000u);
            ASSERT_EQ(f_bitset.slices_count(), skl::integral_ceil(50000u, 64u));

            for (u32 i = 30000u; i < 40000u; ++i) {
                ASSERT_FALSE(f_bitset.test(i));
                ASSERT_FALSE(f_bitset.set(i));
            }
            ASSERT_EQ(f_bitset.count(), 10005u);
        }
    });

    bitset_1_t   bitset_1{4u};
    test_pred_t::operator()(bitset_1);
    bitset_2_t   bitset_2{4u};
    test_pred_t::operator()(bitset_2);
    bitset_3_t   bitset_3{4u};
    test_pred_t::operator()(bitset_3);
    bitset_4_t   bitset_4{4u};
    test_pred_t::operator()(bitset_4);
}
