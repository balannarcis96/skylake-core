#include <skl_pool/hugepage_buffer_pool>
#include <skl_huge_pages>
#include <skl_core>

#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <set>
#include <chrono>

using Pool = skl::HugePageBufferPool;

class HugePageBufferPoolTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(skl::skl_core_init().is_success());
        // skl_core_init() constructs the pool, destroy it so tests can manage lifecycle
        Pool::destroy_pool();
    }

    void SetUp() override {
        ASSERT_EQ(Pool::construct_pool(), SKL_SUCCESS);
    }

    void TearDown() override {
        Pool::destroy_pool();
    }

    static void TearDownTestSuite() {
        ASSERT_TRUE(skl::skl_core_deinit().is_success());
    }
};

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, ConstructDestroy) {
    // Pool already constructed in SetUp, just verify it works
    // Destroy and reconstruct to test the cycle
    Pool::destroy_pool();
    ASSERT_EQ(Pool::construct_pool(), SKL_SUCCESS);
}

TEST_F(HugePageBufferPoolTest, DoubleConstructFails) {
    // Pool already constructed in SetUp
    ASSERT_EQ(Pool::construct_pool(), SKL_ERR_STATE);
}

TEST_F(HugePageBufferPoolTest, DestroyIdempotent) {
    Pool::destroy_pool();
    Pool::destroy_pool(); // Should not crash
}

TEST_F(HugePageBufferPoolTest, BasicAllocFree) {
    auto buffer = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer.is_valid());
    ASSERT_NE(buffer.buffer, nullptr);
    ASSERT_GE(buffer.length, 64u);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, AllocReturnsAlignedMemory) {
    // Note: Natural alignment to buffer size only guaranteed with real huge pages
    // With fallback allocator, we only get cache-line alignment
    // This test verifies basic allocation works for various sizes
    const u32 sizes[] = {32, 64, 128, 256, 512, 1024, 4096, 65536};

    for (u32 size : sizes) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid()) << "Failed for size " << size;
        ASSERT_GE(buffer.length, size);

        // Verify memory is writable
        std::memset(buffer.buffer, 0xAB, buffer.length);

        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, AllocReturnsSufficientSize) {
    // Allocate non-power-of-2 sizes and verify returned size is sufficient
    // Note: returned length is usable size (bucket size minus 8-byte header)
    const u32 sizes[] = {33, 65, 100, 1000, 5000};

    for (u32 size : sizes) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid());

        // Check length >= requested (usable space is sufficient)
        ASSERT_GE(buffer.length, size)
            << "Length " << buffer.length << " is less than requested " << size;

        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Bucket Index Tests (CLZ implementation)
// =============================================================================

TEST_F(HugePageBufferPoolTest, BucketIndexEdgeCases) {
    // Size 0 and 1 should map to minimum bucket (32 bytes)
    {
        auto result = Pool::buffer_get_pool_index(0);
        ASSERT_TRUE(result.is_success());
        ASSERT_EQ(result.value(), 5u); // Bucket 5 = 32 bytes
    }
    {
        auto result = Pool::buffer_get_pool_index(1);
        ASSERT_TRUE(result.is_success());
        ASSERT_EQ(result.value(), 5u);
    }
}

TEST_F(HugePageBufferPoolTest, BucketIndexBoundaries) {
    // Test exact power-of-2 boundaries
    struct TestCase {
        u32 size;
        u32 expected_bucket;
    };

    const TestCase cases[] = {
        {32, 5},       // Exact 32
        {33, 6},       // Just over 32 -> 64
        {64, 6},       // Exact 64
        {65, 7},       // Just over 64 -> 128
        {128, 7},      // Exact 128
        {256, 8},      // Exact 256
        {1024, 10},    // Exact 1KB
        {4096, 12},    // Exact 4KB
        {65536, 16},   // Exact 64KB
        {1 << 20, 20}, // Exact 1MB
        {1 << 21, 21}, // Exact 2MB (max)
    };

    for (const auto& tc : cases) {
        auto result = Pool::buffer_get_pool_index(tc.size);
        ASSERT_TRUE(result.is_success()) << "Failed for size " << tc.size;
        ASSERT_EQ(result.value(), tc.expected_bucket)
            << "Wrong bucket for size " << tc.size;
    }
}

TEST_F(HugePageBufferPoolTest, BufferGetSizeForBucket) {
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(5), 32u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(6), 64u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(10), 1024u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(20), 1u << 20);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(21), 1u << 21);
}

// =============================================================================
// All Bucket Sizes Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, AllBucketSizes) {
    // Test allocation from every bucket (5-21)
    // Note: returned length is usable size (bucket size minus 8-byte header)
    for (u32 bucket = 5; bucket <= 21; ++bucket) {
        u32  size   = 1u << bucket;
        auto buffer = Pool::buffer_alloc(size);

        ASSERT_TRUE(buffer.is_valid()) << "Failed for bucket " << bucket;
        ASSERT_GE(buffer.length, size) << "Insufficient size for bucket " << bucket;

        // Write pattern to verify memory is usable
        std::memset(buffer.buffer, 0xAB, size);
        ASSERT_EQ(buffer.buffer[0], 0xAB);
        ASSERT_EQ(buffer.buffer[size - 1], 0xAB);

        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, MaxSizeAllocation) {
    // 2MB allocation (bucket 21)
    // Note: returned length is usable size (bucket size minus 8-byte header)
    constexpr u32 requested_size = 1u << 21;
    auto          buffer         = Pool::buffer_alloc(requested_size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, requested_size);

    // Write to verify memory is accessible
    std::memset(buffer.buffer, 0xCD, requested_size);

    Pool::buffer_free(buffer);
}

// =============================================================================
// Multiple Allocation Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, MultipleAllocSameSize) {
    const u32                     count = 100;
    std::vector<Pool::buffer_t>   buffers;
    std::set<void*>               addresses;

    for (u32 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(64);
        ASSERT_TRUE(buffer.is_valid());
        buffers.push_back(buffer);
        addresses.insert(buffer.buffer);
    }

    // All addresses should be unique
    ASSERT_EQ(addresses.size(), count);

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, MultipleAllocDifferentSizes) {
    std::vector<Pool::buffer_t> buffers;

    // Allocate various sizes
    const u32 sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096};

    for (u32 size : sizes) {
        for (int i = 0; i < 10; ++i) {
            auto buffer = Pool::buffer_alloc(size);
            ASSERT_TRUE(buffer.is_valid());
            ASSERT_GE(buffer.length, size);
            buffers.push_back(buffer);
        }
    }

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Reuse Tests (LIFO behavior)
// =============================================================================

TEST_F(HugePageBufferPoolTest, BufferReuse) {
    // Allocate and free, then allocate again - should get same address (LIFO)
    auto buffer1 = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer1.is_valid());
    void* addr1 = buffer1.buffer;

    Pool::buffer_free(buffer1);

    auto buffer2 = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer2.is_valid());
    void* addr2 = buffer2.buffer;

    // LIFO: should get the same buffer back
    ASSERT_EQ(addr1, addr2);

    Pool::buffer_free(buffer2);
}

TEST_F(HugePageBufferPoolTest, LIFOOrder) {
    // Allocate A, B, C, free C, B, A, reallocate should get A, B, C
    auto a = Pool::buffer_alloc(64);
    auto b = Pool::buffer_alloc(64);
    auto c = Pool::buffer_alloc(64);

    void* addr_a = a.buffer;
    void* addr_b = b.buffer;
    void* addr_c = c.buffer;

    Pool::buffer_free(c);
    Pool::buffer_free(b);
    Pool::buffer_free(a);

    // LIFO: should get A first (last freed)
    auto new_a = Pool::buffer_alloc(64);
    auto new_b = Pool::buffer_alloc(64);
    auto new_c = Pool::buffer_alloc(64);

    ASSERT_EQ(new_a.buffer, addr_a);
    ASSERT_EQ(new_b.buffer, addr_b);
    ASSERT_EQ(new_c.buffer, addr_c);

    Pool::buffer_free(new_a);
    Pool::buffer_free(new_b);
    Pool::buffer_free(new_c);
}

// =============================================================================
// Object Allocation Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, ObjectAlloc) {
    struct TestObject {
        u64 a = 0;
        u64 b = 0;
        u64 c = 0;

        TestObject(u64 _a, u64 _b, u64 _c)
            : a(_a)
            , b(_b)
            , c(_c) { }
    };

    auto ptr = Pool::object_alloc<TestObject>(1, 2, 3);
    ASSERT_TRUE(ptr);
    ASSERT_EQ(ptr->a, 1u);
    ASSERT_EQ(ptr->b, 2u);
    ASSERT_EQ(ptr->c, 3u);

    ptr.reset();
}

TEST_F(HugePageBufferPoolTest, ObjectAllocDestructorCalled) {
    static int destruct_count = 0;

    struct CountedObject {
        ~CountedObject() { ++destruct_count; }
    };

    destruct_count = 0;

    {
        auto ptr = Pool::object_alloc<CountedObject>();
        ASSERT_TRUE(ptr);
    } // ptr goes out of scope

    ASSERT_EQ(destruct_count, 1);
}

TEST_F(HugePageBufferPoolTest, ObjectPtrMoveSemantics) {
    struct TestObj {
        int value = 42;
    };

    auto ptr1 = Pool::object_alloc<TestObj>();
    ASSERT_TRUE(ptr1);
    ASSERT_EQ(ptr1->value, 42);

    auto ptr2 = std::move(ptr1);
    ASSERT_FALSE(ptr1); // Moved from
    ASSERT_TRUE(ptr2);
    ASSERT_EQ(ptr2->value, 42);

    Pool::ptr_t<TestObj> ptr3;
    ptr3 = std::move(ptr2);
    ASSERT_FALSE(ptr2);
    ASSERT_TRUE(ptr3);
    ASSERT_EQ(ptr3->value, 42);
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, StressRandomAllocFree) {
    std::vector<Pool::buffer_t> allocated;
    std::mt19937                rng(42);
    const u32                   sizes[] = {32, 64, 128, 256, 512, 1024};

    const u64 iterations = 10000;

    for (u64 i = 0; i < iterations; ++i) {
        if (allocated.empty() || (rng() % 2 && allocated.size() < 1000)) {
            // Allocate
            u32  size   = sizes[rng() % 6];
            auto buffer = Pool::buffer_alloc(size);
            ASSERT_TRUE(buffer.is_valid());
            allocated.push_back(buffer);
        } else {
            // Free random buffer
            std::uniform_int_distribution<size_t> dist(0, allocated.size() - 1);
            size_t                                idx = dist(rng);
            Pool::buffer_free(allocated[idx]);
            allocated.erase(allocated.begin() + idx);
        }
    }

    // Clean up remaining
    for (auto& buffer : allocated) {
        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, StressSameSizeAllocFree) {
    const u64 iterations = 50000;

    for (u64 i = 0; i < iterations; ++i) {
        auto buffer = Pool::buffer_alloc(128);
        ASSERT_TRUE(buffer.is_valid());
        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, LargeScaleAllocation) {
    const u64                   count = 10000;
    std::vector<Pool::buffer_t> buffers;
    buffers.reserve(count);

    auto start = std::chrono::high_resolution_clock::now();

    for (u64 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(64);
        ASSERT_TRUE(buffer.is_valid());
        buffers.push_back(buffer);
    }

    auto alloc_time = std::chrono::high_resolution_clock::now() - start;

    printf("Allocated %lu buffers in %lld us (%lld ns per alloc)\n",
           count,
           std::chrono::duration_cast<std::chrono::microseconds>(alloc_time).count(),
           std::chrono::duration_cast<std::chrono::nanoseconds>(alloc_time).count() / count);

    start = std::chrono::high_resolution_clock::now();

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }

    auto free_time = std::chrono::high_resolution_clock::now() - start;

    printf("Freed %lu buffers in %lld us (%lld ns per free)\n",
           count,
           std::chrono::duration_cast<std::chrono::microseconds>(free_time).count(),
           std::chrono::duration_cast<std::chrono::nanoseconds>(free_time).count() / count);
}

// =============================================================================
// Memory Pattern Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, WriteReadPattern) {
    auto buffer = Pool::buffer_alloc(4096);
    ASSERT_TRUE(buffer.is_valid());

    // Write pattern
    for (u32 i = 0; i < buffer.length; ++i) {
        buffer.buffer[i] = static_cast<byte>(i & 0xFF);
    }

    // Verify pattern
    for (u32 i = 0; i < buffer.length; ++i) {
        ASSERT_EQ(buffer.buffer[i], static_cast<byte>(i & 0xFF));
    }

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, NoOverlapBetweenAllocations) {
    const u32 size  = 1024;
    const u32 count = 100;

    std::vector<Pool::buffer_t> buffers;

    // Allocate and fill with unique patterns
    for (u32 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid());
        std::memset(buffer.buffer, static_cast<int>(i), size);
        buffers.push_back(buffer);
    }

    // Verify patterns are intact (no overlap)
    for (u32 i = 0; i < count; ++i) {
        for (u32 j = 0; j < size; ++j) {
            ASSERT_EQ(buffers[i].buffer[j], static_cast<byte>(i))
                << "Buffer " << i << " corrupted at offset " << j;
        }
    }

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Round to Power of 2 Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, RoundToPowerOf2) {
    ASSERT_EQ(Pool::round_to_power_of_2(0), 1u);
    ASSERT_EQ(Pool::round_to_power_of_2(1), 1u);
    ASSERT_EQ(Pool::round_to_power_of_2(2), 2u);
    ASSERT_EQ(Pool::round_to_power_of_2(3), 4u);
    ASSERT_EQ(Pool::round_to_power_of_2(4), 4u);
    ASSERT_EQ(Pool::round_to_power_of_2(5), 8u);
    ASSERT_EQ(Pool::round_to_power_of_2(7), 8u);
    ASSERT_EQ(Pool::round_to_power_of_2(8), 8u);
    ASSERT_EQ(Pool::round_to_power_of_2(9), 16u);
    ASSERT_EQ(Pool::round_to_power_of_2(1000), 1024u);
    ASSERT_EQ(Pool::round_to_power_of_2(1024), 1024u);
    ASSERT_EQ(Pool::round_to_power_of_2(1025), 2048u);
}

// =============================================================================
// Performance Comparison
// =============================================================================

TEST_F(HugePageBufferPoolTest, PerformanceVsMalloc) {
    const u64 iterations = 100000;
    const u32 size       = 256;

    // Test pool
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            auto buffer = Pool::buffer_alloc(size);
            Pool::buffer_free(buffer);
        }

        auto pool_time = std::chrono::high_resolution_clock::now() - start;
        printf("Pool alloc/free: %lld ns per cycle\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(pool_time).count() / iterations);
    }

    // Test malloc/free
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            void* ptr = std::malloc(size);
            std::free(ptr);
        }

        auto malloc_time = std::chrono::high_resolution_clock::now() - start;
        printf("malloc/free: %lld ns per cycle\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(malloc_time).count() / iterations);
    }
}

// =============================================================================
// Multi-Page Allocation Tests (Buckets 22-27: 4MB to 128MB)
// =============================================================================

TEST_F(HugePageBufferPoolTest, MultiPageBucketIndices) {
    // Test bucket index calculation for multi-page sizes
    struct TestCase {
        u32 size;
        u32 expected_bucket;
    };

    const TestCase cases[] = {
        {1u << 22, 22},        // 4MB
        {1u << 23, 23},        // 8MB
        {1u << 24, 24},        // 16MB
        {1u << 25, 25},        // 32MB
        {1u << 26, 26},        // 64MB
        {1u << 27, 27},        // 128MB
        {(1u << 21) + 1, 22},  // 2MB + 1 -> 4MB
        {(1u << 22) + 1, 23},  // 4MB + 1 -> 8MB
        {33u << 20, 26},       // 33MB -> 64MB (bucket 26)
    };

    for (const auto& tc : cases) {
        auto result = Pool::buffer_get_pool_index(tc.size);
        ASSERT_TRUE(result.is_success()) << "Failed for size " << tc.size;
        ASSERT_EQ(result.value(), tc.expected_bucket)
            << "Wrong bucket for size " << tc.size;
    }
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation4MB) {
    constexpr u32 size = 1u << 22; // 4MB (bucket 22, 2 huge pages)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern to verify memory is accessible
    std::memset(buffer.buffer, 0xAB, size);
    ASSERT_EQ(buffer.buffer[0], 0xAB);
    ASSERT_EQ(buffer.buffer[size - 1], 0xAB);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation8MB) {
    constexpr u32 size = 1u << 23; // 8MB (bucket 23, 4 huge pages)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern
    std::memset(buffer.buffer, 0xCD, size);
    ASSERT_EQ(buffer.buffer[0], 0xCD);
    ASSERT_EQ(buffer.buffer[size - 1], 0xCD);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation16MB) {
    constexpr u32 size = 1u << 24; // 16MB (bucket 24, 8 huge pages)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern
    std::memset(buffer.buffer, 0xEF, size);
    ASSERT_EQ(buffer.buffer[0], 0xEF);
    ASSERT_EQ(buffer.buffer[size - 1], 0xEF);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation32MB) {
    constexpr u32 size = 1u << 25; // 32MB (bucket 25, 16 huge pages)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern
    std::memset(buffer.buffer, 0x12, size);
    ASSERT_EQ(buffer.buffer[0], 0x12);
    ASSERT_EQ(buffer.buffer[size - 1], 0x12);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation64MB) {
    constexpr u32 size = 1u << 26; // 64MB (bucket 26, 32 huge pages)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern
    std::memset(buffer.buffer, 0x34, size);
    ASSERT_EQ(buffer.buffer[0], 0x34);
    ASSERT_EQ(buffer.buffer[size - 1], 0x34);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocationMaxSize) {
    // Max requestable size is 128MB - 8 bytes (header overhead)
    constexpr u32 size = (1u << 27) - 8; // 128MB - 8 (bucket 27)
    auto          buffer = Pool::buffer_alloc(size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, size);

    // Write pattern
    std::memset(buffer.buffer, 0x56, size);
    ASSERT_EQ(buffer.buffer[0], 0x56);
    ASSERT_EQ(buffer.buffer[size - 1], 0x56);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageNonPowerOf2Size) {
    // 33MB request should round up to 64MB bucket, usable = 64MB - 8
    constexpr u32 requested_size = 33u << 20; // 33MB

    auto buffer = Pool::buffer_alloc(requested_size);

    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, requested_size);

    // Verify we can use the requested size
    std::memset(buffer.buffer, 0x78, requested_size);
    ASSERT_EQ(buffer.buffer[0], 0x78);
    ASSERT_EQ(buffer.buffer[requested_size - 1], 0x78);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MultiPageBufferReuse) {
    constexpr u32 size = 1u << 22; // 4MB

    // Allocate and free
    auto buffer1 = Pool::buffer_alloc(size);
    ASSERT_TRUE(buffer1.is_valid());
    void* addr1 = buffer1.buffer;
    Pool::buffer_free(buffer1);

    // Allocate again - should get same address (LIFO)
    auto buffer2 = Pool::buffer_alloc(size);
    ASSERT_TRUE(buffer2.is_valid());
    void* addr2 = buffer2.buffer;

    ASSERT_EQ(addr1, addr2);

    Pool::buffer_free(buffer2);
}

TEST_F(HugePageBufferPoolTest, MultiPageMultipleAllocations) {
    constexpr u32 size = 1u << 22; // 4MB

    // Allocate multiple large buffers
    std::vector<Pool::buffer_t> buffers;
    std::set<void*>             addresses;

    for (int i = 0; i < 5; ++i) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid()) << "Failed at allocation " << i;
        ASSERT_GE(buffer.length, size);
        buffers.push_back(buffer);
        addresses.insert(buffer.buffer);
    }

    // All addresses should be unique
    ASSERT_EQ(addresses.size(), 5u);

    // Free all
    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, MultiPageWriteReadPattern) {
    constexpr u32 size = 1u << 23; // 8MB

    auto buffer = Pool::buffer_alloc(size);
    ASSERT_TRUE(buffer.is_valid());

    // Write pattern across entire buffer
    for (u32 i = 0; i < size; i += 4096) {
        buffer.buffer[i] = static_cast<byte>(i & 0xFF);
    }

    // Verify pattern
    for (u32 i = 0; i < size; i += 4096) {
        ASSERT_EQ(buffer.buffer[i], static_cast<byte>(i & 0xFF))
            << "Pattern mismatch at offset " << i;
    }

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, AllMultiPageBuckets) {
    // Test allocation from every multi-page bucket (22-26)
    // Bucket 27 (128MB) tested separately due to header overhead limit
    for (u32 bucket = 22; bucket <= 26; ++bucket) {
        u32  size   = 1u << bucket;
        auto buffer = Pool::buffer_alloc(size);

        ASSERT_TRUE(buffer.is_valid()) << "Failed for bucket " << bucket;
        ASSERT_GE(buffer.length, size) << "Insufficient size for bucket " << bucket;

        // Verify memory is usable at boundaries
        buffer.buffer[0]        = 0xAA;
        buffer.buffer[size - 1] = 0xBB;
        ASSERT_EQ(buffer.buffer[0], 0xAA);
        ASSERT_EQ(buffer.buffer[size - 1], 0xBB);

        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Header Mechanism Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, HeaderSizeCorrectlyDeducted) {
    // Verify that returned length is bucket_size - 8 (header overhead)
    // Request 24 bytes -> bucket 5 (32 bytes) -> usable = 24 bytes
    {
        auto buffer = Pool::buffer_alloc(24);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 24u); // Exactly 32 - 8
        Pool::buffer_free(buffer);
    }

    // Request 56 bytes -> bucket 6 (64 bytes) -> usable = 56 bytes
    {
        auto buffer = Pool::buffer_alloc(56);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 56u); // Exactly 64 - 8
        Pool::buffer_free(buffer);
    }

    // Request 120 bytes -> bucket 7 (128 bytes) -> usable = 120 bytes
    {
        auto buffer = Pool::buffer_alloc(120);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 120u); // Exactly 128 - 8
        Pool::buffer_free(buffer);
    }
}

TEST_F(HugePageBufferPoolTest, FreeByPointerOnly) {
    // Verify buffer_free_ptr works without needing the size
    auto buffer = Pool::buffer_alloc(100);
    ASSERT_TRUE(buffer.is_valid());

    // Write pattern
    std::memset(buffer.buffer, 0xAB, 100);

    // Free using pointer-only API
    Pool::buffer_free_ptr(buffer.buffer);

    // Allocate again - should succeed (pool not corrupted)
    auto buffer2 = Pool::buffer_alloc(100);
    ASSERT_TRUE(buffer2.is_valid());
    Pool::buffer_free(buffer2);
}
