//!
//! \file test_hugepage_sanity.cpp
//!
//! \brief Hugepage sanity check for SPSC bidirectional ring
//!
#include <skl_spsc_bidirectional_ring>
#include <skl_huge_pages>
#include <skl_core>

#include <gtest/gtest.h>

// Comprehensive hugepage sanity check
// Verifies hugepage size, raw allocation, and ring lifecycle
TEST(SPSCBidirectionalRingHugepageSanity, SanityCheck) {
    ASSERT_TRUE(skl::skl_core_init().is_success());

    if (!skl::huge_pages::is_huge_pages_enabled()) {
        ASSERT_TRUE(skl::skl_core_deinit().is_success());
        GTEST_SKIP() << "Hugepages not available";
    }

    // Assert hugepage size is exactly 2MB
    constexpr u64 CExpectedHugePageSize = 2ULL * 1024ULL * 1024ULL; // 2MB
    const u64 system_hugepage_size = skl::huge_pages::get_system_huge_page_size();
    SKL_ASSERT_PERMANENT(system_hugepage_size == CExpectedHugePageSize
                         && "System hugepage size must be exactly 2MB");
    ASSERT_EQ(system_hugepage_size, CExpectedHugePageSize);
    ASSERT_EQ(skl::huge_pages::CHugePageSize, CExpectedHugePageSize);

    // Test raw hugepage allocation/deallocation
    {
        void* raw_page = skl::huge_pages::skl_huge_page_alloc(1);
        SKL_ASSERT_PERMANENT(raw_page != nullptr && "Hugepage allocation failed");
        ASSERT_NE(raw_page, nullptr);

        // Write pattern to verify memory is usable
        auto* bytes = static_cast<u8*>(raw_page);
        bytes[0] = 0xDE;
        bytes[CExpectedHugePageSize - 1] = 0xAD;
        ASSERT_EQ(bytes[0], 0xDE);
        ASSERT_EQ(bytes[CExpectedHugePageSize - 1], 0xAD);

        skl::huge_pages::skl_huge_page_free(raw_page, 1);
    }

    // Test ring instantiation and destruction with hugepages
    {
        using ring_t = skl::spsc_bidirectional_ring_t<u64, 128, true>;

        ring_t ring{};
        ASSERT_FALSE(ring.has_internal_storage());

        ring.allocate_internal_storage();
        SKL_ASSERT_PERMANENT(ring.has_internal_storage() && "Ring hugepage storage allocation failed");
        ASSERT_TRUE(ring.has_internal_storage());

        // Basic operations
        auto* obj = ring.allocate();
        SKL_ASSERT_PERMANENT(obj != nullptr && "Ring allocation failed");
        *obj = 0xCAFEBABE;
        ring.submit();

        u64* dequeued[1];
        ASSERT_EQ(ring.dequeue_burst(dequeued, 1), 1u);
        ASSERT_EQ(*dequeued[0], 0xCAFEBABE);

        ring.free_internal_storage();
        ASSERT_FALSE(ring.has_internal_storage());
    }

    // Verify ring destructor handles cleanup (no explicit free)
    {
        using ring_t = skl::spsc_bidirectional_ring_t<u64, 64, true>;
        ring_t ring{};
        ring.allocate_internal_storage();
        ASSERT_TRUE(ring.has_internal_storage());
        // Destructor should free hugepage storage automatically
    }

    ASSERT_TRUE(skl::skl_core_deinit().is_success());
}
