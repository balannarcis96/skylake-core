#include <skl_fixed_vector_if>
#include <skl_log>

#include <gtest/gtest.h>

TEST(SkylakeFixedVector, AddRangeFrom_EmptySourceReturnsZero) {
    skl::skl_fixed_vector<int, 10> source;
    skl::skl_fixed_vector<int, 10> dest;

    dest.upgrade().emplace_back(1);
    dest.upgrade().emplace_back(2);

    auto&    dest_if = dest.upgrade();
    uint32_t added   = dest_if.add_range_from(source);

    ASSERT_EQ(added, 0U);
    ASSERT_EQ(dest.size(), 2U);
    ASSERT_EQ(source.size(), 0U);
}

TEST(SkylakeFixedVector, AddRangeFrom_FullDestinationReturnsZero) {
    skl::skl_fixed_vector<int, 5> source;
    skl::skl_fixed_vector<int, 3> dest;

    source.upgrade().emplace_back(10);
    source.upgrade().emplace_back(20);

    dest.upgrade().emplace_back(1);
    dest.upgrade().emplace_back(2);
    dest.upgrade().emplace_back(3);

    auto&    dest_if = dest.upgrade();
    uint32_t added   = dest_if.add_range_from(source);

    ASSERT_EQ(added, 0U);
    ASSERT_EQ(dest.size(), 3U);
    ASSERT_EQ(source.size(), 2U);
    ASSERT_EQ(source[0], 10);
    ASSERT_EQ(source[1], 20);
}

TEST(SkylakeFixedVector, AddRangeFrom_CompleteTransfer) {
    skl::skl_fixed_vector<int, 10> source;
    skl::skl_fixed_vector<int, 10> dest;

    source.upgrade().emplace_back(100);
    source.upgrade().emplace_back(200);
    source.upgrade().emplace_back(300);

    dest.upgrade().emplace_back(1);
    dest.upgrade().emplace_back(2);

    auto&    dest_if = dest.upgrade();
    uint32_t added   = dest_if.add_range_from(source);

    ASSERT_EQ(added, 3U);
    ASSERT_EQ(dest.size(), 5U);
    ASSERT_EQ(source.size(), 0U);
    ASSERT_EQ(dest[2], 100);
    ASSERT_EQ(dest[3], 200);
    ASSERT_EQ(dest[4], 300);
}

TEST(SkylakeFixedVector, AddRangeFrom_PartialTransferLeavesRemainder) {
    skl::skl_fixed_vector<int, 10> source;
    skl::skl_fixed_vector<int, 5>  dest;

    source.upgrade().emplace_back(10);
    source.upgrade().emplace_back(20);
    source.upgrade().emplace_back(30);
    source.upgrade().emplace_back(40);
    source.upgrade().emplace_back(50);

    dest.upgrade().emplace_back(1);
    dest.upgrade().emplace_back(2);

    auto&    dest_if = dest.upgrade();
    uint32_t added   = dest_if.add_range_from(source);

    ASSERT_EQ(added, 3U);
    ASSERT_EQ(dest.size(), 5U);
    ASSERT_EQ(source.size(), 2U);
    ASSERT_EQ(dest[2], 10);
    ASSERT_EQ(dest[3], 20);
    ASSERT_EQ(dest[4], 30);
    ASSERT_EQ(source[0], 40);
    ASSERT_EQ(source[1], 50);
}

TEST(SkylakeFixedVector, AddRangeFrom_MultipleCallsDrainSource) {
    skl::skl_fixed_vector<int, 10> source;
    skl::skl_fixed_vector<int, 4>  dest;

    for (int i = 1; i <= 7; ++i) {
        source.upgrade().emplace_back(i * 10);
    }

    auto& dest_if = dest.upgrade();

    // First call - transfers 4 elements (fills dest)
    uint32_t added1 = dest_if.add_range_from(source);
    ASSERT_EQ(added1, 4U);
    ASSERT_EQ(dest.size(), 4U);
    ASSERT_EQ(source.size(), 3U);

    // Clear dest for second transfer
    dest.clear();

    // Second call - transfers remaining 3 elements
    uint32_t added2 = dest_if.add_range_from(source);
    ASSERT_EQ(added2, 3U);
    ASSERT_EQ(dest.size(), 3U);
    ASSERT_EQ(source.size(), 0U);
    ASSERT_EQ(dest[0], 50);
    ASSERT_EQ(dest[1], 60);
    ASSERT_EQ(dest[2], 70);
}
