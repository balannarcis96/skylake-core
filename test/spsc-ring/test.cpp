#include <skl_spsc_unidirectional_ring>
#include <skl_huge_pages>

#include <skl_thread>
#include <skl_core>
#include <skl_signal>
#include <skl_epoch>
#include <skl_sleep>

#include <gtest/gtest.h>

namespace {
struct my_object_t {
    u64  numbers[64U];
    u64  order;
    bool is_free = true;

    void reset() noexcept {
        is_free = true;
    }
};

// Test object with non-trivial constructor/destructor for hugepage tests
struct tracked_object_t {
    static inline u32 construct_count = 0;
    static inline u32 destruct_count  = 0;

    u64 value = 0;

    tracked_object_t() noexcept { ++construct_count; }
    ~tracked_object_t() noexcept { ++destruct_count; }

    tracked_object_t(const tracked_object_t&) = default;
    tracked_object_t& operator=(const tracked_object_t&) = default;
    tracked_object_t(tracked_object_t&&) = default;
    tracked_object_t& operator=(tracked_object_t&&) = default;

    void reset() noexcept { value = 0; }

    static void reset_counters() noexcept {
        construct_count = 0;
        destruct_count  = 0;
    }
};
} // namespace

// =============================================================================
// Hugepage Memory Management Tests for Unidirectional Ring
// =============================================================================

class SPSCUnidirectionalRingHugepageTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(skl::skl_core_init().is_success());
    }

    static void TearDownTestSuite() {
        ASSERT_TRUE(skl::skl_core_deinit().is_success());
    }

    void SetUp() override {
        tracked_object_t::reset_counters();
    }
};

// Test: Hugepage allocation and deallocation with trivial type
TEST_F(SPSCUnidirectionalRingHugepageTest, TrivialTypeAllocation) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    using ring_t = skl::spsc_unidirectional_ring_t<u64, 64, true>;
    ring_t ring{};

    // Verify storage not allocated initially
    ASSERT_FALSE(ring.has_internal_storage());

    // Allocate storage
    ring.allocate_internal_storage();
    ASSERT_TRUE(ring.has_internal_storage());

    // Verify buffer is accessible
    u64* buffer = ring.buffer();
    ASSERT_NE(buffer, nullptr);

    // Write to verify memory is usable
    for (u64 i = 0; i < ring.Size; ++i) {
        buffer[i] = i;
    }
    for (u64 i = 0; i < ring.Size; ++i) {
        ASSERT_EQ(buffer[i], i);
    }

    // Free storage
    ring.free_internal_storage();
    ASSERT_FALSE(ring.has_internal_storage());
}

// Test: Hugepage allocation with non-trivial type (constructor/destructor tracking)
TEST_F(SPSCUnidirectionalRingHugepageTest, NonTrivialTypeConstruction) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    constexpr u64 Size = 128;
    using ring_t = skl::spsc_unidirectional_ring_t<tracked_object_t, Size, true>;

    tracked_object_t::reset_counters();

    {
        ring_t ring{};

        // No constructions yet
        ASSERT_EQ(tracked_object_t::construct_count, 0u);

        // Allocate - should construct all objects
        ring.allocate_internal_storage();
        ASSERT_EQ(tracked_object_t::construct_count, Size);
        ASSERT_EQ(tracked_object_t::destruct_count, 0u);

        // Free - should destruct all objects
        ring.free_internal_storage();
        ASSERT_EQ(tracked_object_t::destruct_count, Size);
    }
}

// Test: Destructor automatically frees hugepage storage
TEST_F(SPSCUnidirectionalRingHugepageTest, DestructorFreesStorage) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    constexpr u64 Size = 64;
    using ring_t = skl::spsc_unidirectional_ring_t<tracked_object_t, Size, true>;

    tracked_object_t::reset_counters();

    {
        ring_t ring{};
        ring.allocate_internal_storage();
        ASSERT_EQ(tracked_object_t::construct_count, Size);
        // Don't call free_internal_storage - let destructor handle it
    }

    // Destructor should have called destructors
    ASSERT_EQ(tracked_object_t::destruct_count, Size);
}

// Test: Double allocation protection (death test)
TEST_F(SPSCUnidirectionalRingHugepageTest, DoubleAllocationDeath) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    using ring_t = skl::spsc_unidirectional_ring_t<u64, 64, true>;

    ASSERT_DEATH(
        {
            ring_t ring{};
            ring.allocate_internal_storage();
            ring.allocate_internal_storage(); // Should die here
        },
        ".*");
}

// Test: Use without allocation protection (death test)
TEST_F(SPSCUnidirectionalRingHugepageTest, UseWithoutAllocationDeath) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    using ring_t = skl::spsc_unidirectional_ring_t<u64, 64, true>;

    // allocate() without storage
    ASSERT_DEATH(
        {
            ring_t ring{};
            (void)ring.allocate();
        },
        ".*");

    // buffer() without storage
    ASSERT_DEATH(
        {
            ring_t ring{};
            (void)ring.buffer();
        },
        ".*");
}

// Test: Use-after-free protection (death test)
TEST_F(SPSCUnidirectionalRingHugepageTest, UseAfterFreeDeath) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    using ring_t = skl::spsc_unidirectional_ring_t<u64, 64, true>;

    ASSERT_DEATH(
        {
            ring_t ring{};
            ring.allocate_internal_storage();
            ring.free_internal_storage();
            (void)ring.allocate(); // Should die here - use after free
        },
        ".*");
}

// Test: Full producer-consumer flow with hugepages
TEST_F(SPSCUnidirectionalRingHugepageTest, ProducerConsumerFlow) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    constexpr u64 Size = 256;
    using ring_t = skl::spsc_unidirectional_ring_t<u64, Size, true>;

    ring_t ring{};
    ring.allocate_internal_storage();

    // Producer: allocate and submit objects
    constexpr u32 batch_size = 16;
    u64* objects[batch_size];

    ASSERT_TRUE(ring.allocate_bulk(objects, batch_size));
    for (u32 i = 0; i < batch_size; ++i) {
        *objects[i] = u64{i} * 100u;
    }
    ring.submit();

    // Consumer: dequeue and process
    u64* dequeued[batch_size];
    u32 count = ring.dequeue_burst(dequeued, batch_size);
    ASSERT_EQ(count, batch_size);

    for (u32 i = 0; i < count; ++i) {
        ASSERT_EQ(*dequeued[i], u64{i} * 100u);
    }

    // Free processed items
    ring.free_processed();

    // Verify free count is back to full
    ASSERT_EQ(ring.free_count(), Size);
}

// Test: Large hugepage allocation (multiple pages)
TEST_F(SPSCUnidirectionalRingHugepageTest, LargeAllocation) {
    if (!skl::huge_pages::is_huge_pages_enabled()) {
        GTEST_SKIP() << "Hugepages not available";
    }

    // Create a ring that requires multiple hugepages
    // Each hugepage is 2MB, so with 4KB objects and 1024 elements = 4MB = 2 hugepages
    struct large_object_t {
        u8 data[4096];
        void reset() noexcept { }
    };

    using ring_t = skl::spsc_unidirectional_ring_t<large_object_t, 1024, true>;

    ring_t ring{};
    ASSERT_GE(ring.CHugePagesCount, 2u); // Should need at least 2 hugepages

    ring.allocate_internal_storage();
    ASSERT_TRUE(ring.has_internal_storage());

    // Verify we can access all elements
    auto* obj = ring.allocate();
    ASSERT_NE(obj, nullptr);
    obj->data[0] = 0xAB;
    obj->data[4095] = 0xCD;
    ASSERT_EQ(obj->data[0], 0xAB);
    ASSERT_EQ(obj->data[4095], 0xCD);

    ring.free_internal_storage();
}

// Test: Fallback allocation when hugepages unavailable
TEST_F(SPSCUnidirectionalRingHugepageTest, FallbackMode) {
    // This test runs regardless of hugepage availability
    // to test the fallback path
    constexpr u64 Size = 64;
    using ring_t = skl::spsc_unidirectional_ring_t<u64, Size, true>;

    ring_t ring{};
    ring.allocate_internal_storage(); // Should work even without hugepages (fallback)
    ASSERT_TRUE(ring.has_internal_storage());

    // Basic operation should work
    auto* obj = ring.allocate();
    ASSERT_NE(obj, nullptr);
    *obj = 42;
    ring.submit();

    u64* dequeued[1];
    ASSERT_EQ(ring.dequeue_burst(dequeued, 1), 1u);
    ASSERT_EQ(*dequeued[0], 42u);
}
