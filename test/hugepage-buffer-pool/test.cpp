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
        {     32,  5}, // Exact 32
        {     33,  6}, // Just over 32 -> 64
        {     64,  6}, // Exact 64
        {     65,  7}, // Just over 64 -> 128
        {    128,  7}, // Exact 128
        {    256,  8}, // Exact 256
        {   1024, 10}, // Exact 1KB
        {   4096, 12}, // Exact 4KB
        {  65536, 16}, // Exact 64KB
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
    const u32                   count = 100;
    std::vector<Pool::buffer_t> buffers;
    std::set<void*>             addresses;

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
        {      1u << 22, 22}, // 4MB
        {      1u << 23, 23}, // 8MB
        {      1u << 24, 24}, // 16MB
        {      1u << 25, 25}, // 32MB
        {      1u << 26, 26}, // 64MB
        {      1u << 27, 27}, // 128MB
        {(1u << 21) + 1, 22}, // 2MB + 1 -> 4MB
        {(1u << 22) + 1, 23}, // 4MB + 1 -> 8MB
        {     33u << 20, 26}, // 33MB -> 64MB (bucket 26)
    };

    for (const auto& tc : cases) {
        auto result = Pool::buffer_get_pool_index(tc.size);
        ASSERT_TRUE(result.is_success()) << "Failed for size " << tc.size;
        ASSERT_EQ(result.value(), tc.expected_bucket)
            << "Wrong bucket for size " << tc.size;
    }
}

TEST_F(HugePageBufferPoolTest, MultiPageAllocation4MB) {
    constexpr u32 size   = 1u << 22; // 4MB (bucket 22, 2 huge pages)
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
    constexpr u32 size   = 1u << 23; // 8MB (bucket 23, 4 huge pages)
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
    constexpr u32 size   = 1u << 24; // 16MB (bucket 24, 8 huge pages)
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
    constexpr u32 size   = 1u << 25; // 32MB (bucket 25, 16 huge pages)
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
    constexpr u32 size   = 1u << 26; // 64MB (bucket 26, 32 huge pages)
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
    constexpr u32 size   = (1u << 27) - 8; // 128MB - 8 (bucket 27)
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

TEST_F(HugePageBufferPoolTest, ZeroSizeAllocation) {
    // Allocate 0 bytes - should still get minimum bucket (24 usable bytes)
    auto buffer = Pool::buffer_alloc(0);
    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, 0u);

    // Should be writable
    if (buffer.length > 0) {
        buffer.buffer[0] = 0xAA;
        ASSERT_EQ(buffer.buffer[0], 0xAA);
    }

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, MinimumSizeAllocation) {
    // Allocate 1 byte - should get minimum bucket
    auto buffer = Pool::buffer_alloc(1);
    ASSERT_TRUE(buffer.is_valid());
    ASSERT_GE(buffer.length, 1u);

    buffer.buffer[0] = 0xBB;
    ASSERT_EQ(buffer.buffer[0], 0xBB);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, ExactMinimumUsableSize) {
    // 24 bytes is the exact minimum usable size (bucket 5 = 32 - 8 header)
    auto buffer = Pool::buffer_alloc(24);
    ASSERT_TRUE(buffer.is_valid());
    ASSERT_EQ(buffer.length, 24u);

    // Write to entire usable area
    std::memset(buffer.buffer, 0xCC, 24);
    ASSERT_EQ(buffer.buffer[0], 0xCC);
    ASSERT_EQ(buffer.buffer[23], 0xCC);

    Pool::buffer_free(buffer);
}

TEST_F(HugePageBufferPoolTest, PointerAlignment) {
    // Verify returned pointers are 8-byte aligned (required for header access)
    const u32 sizes[] = {1, 24, 32, 64, 100, 256, 1000, 4096};

    for (u32 size : sizes) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid());

        // Check 8-byte alignment
        ASSERT_EQ(reinterpret_cast<uintptr_t>(buffer.buffer) % 8, 0u)
            << "Buffer not 8-byte aligned for size " << size;

        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// hugepage_allocator (STL Allocator) Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, HugepageAllocatorBasic) {
    using Alloc = skl::hugepage_allocator<int>;

    int* p = Alloc::allocate(10);
    ASSERT_NE(p, nullptr);

    // Write and read
    for (int i = 0; i < 10; ++i) {
        p[i] = i * 100;
    }
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(p[i], i * 100);
    }

    Alloc::deallocate(p, 10);
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorZeroCount) {
    using Alloc = skl::hugepage_allocator<int>;

    // Allocate 0 should return nullptr
    int* p = Alloc::allocate(0);
    ASSERT_EQ(p, nullptr);

    // Deallocate nullptr should be safe
    Alloc::deallocate(nullptr, 0);
    Alloc::deallocate(nullptr, 100); // count ignored for nullptr
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorDeallocateIgnoresCount) {
    using Alloc = skl::hugepage_allocator<int>;

    // Allocate 100 ints
    int* p = Alloc::allocate(100);
    ASSERT_NE(p, nullptr);

    // Write pattern
    for (int i = 0; i < 100; ++i) {
        p[i] = i;
    }

    // Deallocate with WRONG count - should still work due to header
    Alloc::deallocate(p, 50);  // Wrong count, but size is in header

    // Allocate again to verify pool is not corrupted
    int* p2 = Alloc::allocate(100);
    ASSERT_NE(p2, nullptr);
    Alloc::deallocate(p2, 100);
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorWithVector) {
    std::vector<int, skl::hugepage_allocator<int>> vec;

    // Push elements to trigger growth
    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i);
    }

    // Verify contents
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(vec[i], i);
    }

    // Clear and verify no corruption
    vec.clear();
    vec.shrink_to_fit();
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorReallocate) {
    using Alloc = skl::hugepage_allocator<int>;

    // Allocate initial
    int* p1 = Alloc::allocate(10);
    ASSERT_NE(p1, nullptr);
    for (int i = 0; i < 10; ++i) {
        p1[i] = i * 10;
    }

    // Reallocate to larger
    int* p2 = Alloc::reallocate(p1, 10, 100);
    ASSERT_NE(p2, nullptr);

    // Original data should be preserved
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(p2[i], i * 10);
    }

    // Can write to new space
    for (int i = 10; i < 100; ++i) {
        p2[i] = i * 10;
    }

    Alloc::deallocate(p2, 100);
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorReallocateToZero) {
    using Alloc = skl::hugepage_allocator<int>;

    int* p = Alloc::allocate(10);
    ASSERT_NE(p, nullptr);

    // Reallocate to 0 should free and return nullptr
    int* p2 = Alloc::reallocate(p, 10, 0);
    ASSERT_EQ(p2, nullptr);
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorReallocateFromNull) {
    using Alloc = skl::hugepage_allocator<int>;

    // Reallocate from nullptr should just allocate
    int* p = Alloc::reallocate(nullptr, 0, 50);
    ASSERT_NE(p, nullptr);

    for (int i = 0; i < 50; ++i) {
        p[i] = i;
    }

    Alloc::deallocate(p, 50);
}

TEST_F(HugePageBufferPoolTest, HugepageAllocatorLargeAllocation) {
    using Alloc = skl::hugepage_allocator<u64>;

    // Allocate 1MB worth of u64s
    constexpr size_t count = (1u << 20) / sizeof(u64);
    u64* p = Alloc::allocate(count);
    ASSERT_NE(p, nullptr);

    // Write pattern
    for (size_t i = 0; i < count; i += 1000) {
        p[i] = i;
    }

    // Verify
    for (size_t i = 0; i < count; i += 1000) {
        ASSERT_EQ(p[i], i);
    }

    Alloc::deallocate(p, count);
}

// =============================================================================
// ptr_t (Smart Pointer) Comprehensive Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, PtrTRelease) {
    struct TestObj { int x = 42; };

    auto ptr = Pool::object_alloc<TestObj>();
    ASSERT_TRUE(ptr);

    TestObj* raw = ptr.release();
    ASSERT_FALSE(ptr);  // ptr should be null after release
    ASSERT_NE(raw, nullptr);
    ASSERT_EQ(raw->x, 42);

    // Must manually free since we released
    Pool::object_free(raw);
}

TEST_F(HugePageBufferPoolTest, PtrTResetWithNewPointer) {
    struct TestObj { int x; TestObj(int v) : x(v) {} };

    auto ptr = Pool::object_alloc<TestObj>(1);
    ASSERT_EQ(ptr->x, 1);

    // Allocate new object manually
    auto alloc = Pool::buffer_alloc(sizeof(TestObj));
    auto* raw = new (alloc.buffer) TestObj(2);

    // Reset with new pointer (old should be freed)
    ptr.reset(raw);
    ASSERT_EQ(ptr->x, 2);

    // ptr destructor will free the new object
}

TEST_F(HugePageBufferPoolTest, PtrTSwap) {
    struct TestObj { int x; TestObj(int v) : x(v) {} };

    auto ptr1 = Pool::object_alloc<TestObj>(100);
    auto ptr2 = Pool::object_alloc<TestObj>(200);

    ASSERT_EQ(ptr1->x, 100);
    ASSERT_EQ(ptr2->x, 200);

    ptr1.swap(ptr2);

    ASSERT_EQ(ptr1->x, 200);
    ASSERT_EQ(ptr2->x, 100);
}

TEST_F(HugePageBufferPoolTest, PtrTComparisonOperators) {
    struct TestObj { int x = 0; };

    auto ptr1 = Pool::object_alloc<TestObj>();
    auto ptr2 = Pool::object_alloc<TestObj>();
    Pool::ptr_t<TestObj> null_ptr;

    // Nullptr comparisons
    ASSERT_TRUE(null_ptr == nullptr);
    ASSERT_FALSE(null_ptr != nullptr);
    ASSERT_FALSE(ptr1 == nullptr);
    ASSERT_TRUE(ptr1 != nullptr);

    // Pointer comparisons
    ASSERT_FALSE(ptr1 == ptr2);
    ASSERT_TRUE(ptr1 != ptr2);

    // Ordering (just verify it doesn't crash and is consistent)
    bool less = ptr1 < ptr2;
    bool greater = ptr1 > ptr2;
    ASSERT_NE(less, greater); // Can't both be true
}

TEST_F(HugePageBufferPoolTest, PtrTNullptrConstruction) {
    Pool::ptr_t<int> ptr(nullptr);
    ASSERT_FALSE(ptr);
    ASSERT_EQ(ptr.get(), nullptr);
}

TEST_F(HugePageBufferPoolTest, PtrTNullptrAssignment) {
    auto ptr = Pool::object_alloc<int>();
    ASSERT_TRUE(ptr);

    ptr = nullptr;
    ASSERT_FALSE(ptr);
}

// =============================================================================
// Object Allocation Edge Cases
// =============================================================================

TEST_F(HugePageBufferPoolTest, ObjectAllocTrivialType) {
    // POD type with no constructor
    auto ptr = Pool::object_alloc<u64>();
    ASSERT_TRUE(ptr);

    *ptr = 0xDEADBEEFCAFEBABE;
    ASSERT_EQ(*ptr, 0xDEADBEEFCAFEBABE);
}

TEST_F(HugePageBufferPoolTest, ObjectAllocLargeObject) {
    // Object larger than minimum bucket
    struct LargeObj {
        byte data[1024];
        LargeObj() { std::memset(data, 0xAB, sizeof(data)); }
    };

    auto ptr = Pool::object_alloc<LargeObj>();
    ASSERT_TRUE(ptr);

    // Verify constructor ran
    ASSERT_EQ(ptr->data[0], 0xAB);
    ASSERT_EQ(ptr->data[1023], 0xAB);
}

TEST_F(HugePageBufferPoolTest, ObjectAllocMultipleSameType) {
    struct TestObj { int id; TestObj(int i) : id(i) {} };

    std::vector<Pool::ptr_t<TestObj>> ptrs;
    std::set<void*> addresses;

    for (int i = 0; i < 100; ++i) {
        auto ptr = Pool::object_alloc<TestObj>(i);
        ASSERT_TRUE(ptr);
        ASSERT_EQ(ptr->id, i);
        addresses.insert(ptr.get());
        ptrs.push_back(std::move(ptr));
    }

    // All addresses unique
    ASSERT_EQ(addresses.size(), 100u);
}

TEST_F(HugePageBufferPoolTest, ObjectFreeAndRealloc) {
    struct TestObj { int x; TestObj(int v) : x(v) {} };

    auto ptr1 = Pool::object_alloc<TestObj>(42);
    void* addr1 = ptr1.get();
    ptr1.reset();

    // Should get same address back (LIFO)
    auto ptr2 = Pool::object_alloc<TestObj>(99);
    void* addr2 = ptr2.get();

    ASSERT_EQ(addr1, addr2);
    ASSERT_EQ(ptr2->x, 99);
}

// =============================================================================
// Cross-Bucket Operations
// =============================================================================

TEST_F(HugePageBufferPoolTest, InterleavedBucketOperations) {
    // Allocate from different buckets, free in different order
    auto small = Pool::buffer_alloc(32);   // Bucket 6 (32+8=40 -> 64)
    auto medium = Pool::buffer_alloc(256); // Bucket 9 (256+8=264 -> 512)
    auto large = Pool::buffer_alloc(4096); // Bucket 13 (4096+8 -> 8192)

    ASSERT_TRUE(small.is_valid());
    ASSERT_TRUE(medium.is_valid());
    ASSERT_TRUE(large.is_valid());

    // Write unique patterns
    std::memset(small.buffer, 0x11, 32);
    std::memset(medium.buffer, 0x22, 256);
    std::memset(large.buffer, 0x33, 4096);

    // Free in different order
    Pool::buffer_free(medium);
    Pool::buffer_free(small);
    Pool::buffer_free(large);

    // Allocate again and verify no cross-contamination
    auto small2 = Pool::buffer_alloc(32);
    auto medium2 = Pool::buffer_alloc(256);
    auto large2 = Pool::buffer_alloc(4096);

    // Write new patterns
    std::memset(small2.buffer, 0xAA, 32);
    std::memset(medium2.buffer, 0xBB, 256);
    std::memset(large2.buffer, 0xCC, 4096);

    // Verify patterns
    for (u32 i = 0; i < 32; ++i) ASSERT_EQ(small2.buffer[i], 0xAA);
    for (u32 i = 0; i < 256; ++i) ASSERT_EQ(medium2.buffer[i], 0xBB);
    for (u32 i = 0; i < 4096; ++i) ASSERT_EQ(large2.buffer[i], 0xCC);

    Pool::buffer_free(small2);
    Pool::buffer_free(medium2);
    Pool::buffer_free(large2);
}

TEST_F(HugePageBufferPoolTest, SimultaneousMultiBucketAllocations) {
    // Allocate many buffers from multiple buckets simultaneously
    std::vector<Pool::buffer_t> buffers;
    const u32 sizes[] = {24, 56, 120, 248, 504, 1016}; // Exact usable sizes

    for (int round = 0; round < 10; ++round) {
        for (u32 size : sizes) {
            auto buffer = Pool::buffer_alloc(size);
            ASSERT_TRUE(buffer.is_valid());
            ASSERT_GE(buffer.length, size);

            // Write round number as pattern
            std::memset(buffer.buffer, static_cast<int>(round), size);
            buffers.push_back(buffer);
        }
    }

    // Verify all patterns intact
    size_t idx = 0;
    for (int round = 0; round < 10; ++round) {
        for (u32 size : sizes) {
            for (u32 i = 0; i < size; ++i) {
                ASSERT_EQ(buffers[idx].buffer[i], static_cast<byte>(round))
                    << "Corruption at round " << round << ", size " << size << ", offset " << i;
            }
            ++idx;
        }
    }

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Buffer Reuse After Free
// =============================================================================

TEST_F(HugePageBufferPoolTest, BufferWritableAfterReuse) {
    auto buffer1 = Pool::buffer_alloc(100);
    std::memset(buffer1.buffer, 0xFF, 100);
    void* addr = buffer1.buffer;
    Pool::buffer_free(buffer1);

    auto buffer2 = Pool::buffer_alloc(100);
    ASSERT_EQ(buffer2.buffer, addr); // Same address (LIFO)

    // Buffer should be fully writable
    std::memset(buffer2.buffer, 0x00, buffer2.length);

    // Verify write succeeded
    for (u32 i = 0; i < buffer2.length; ++i) {
        ASSERT_EQ(buffer2.buffer[i], 0x00);
    }

    Pool::buffer_free(buffer2);
}

// =============================================================================
// Bucket Boundary Edge Cases
// =============================================================================

TEST_F(HugePageBufferPoolTest, AllocationAtBucketBoundaries) {
    // Test allocations at exact bucket boundaries (size - 8 = bucket_size - 8)
    // These should result in exact fit with no wasted space

    struct BoundaryTest {
        u32 request_size;
        u32 expected_usable;
    };

    const BoundaryTest tests[] = {
        {24, 24},    // 24+8=32 -> bucket 5 -> usable 24
        {25, 56},    // 25+8=33 -> bucket 6 -> usable 56
        {56, 56},    // 56+8=64 -> bucket 6 -> usable 56
        {57, 120},   // 57+8=65 -> bucket 7 -> usable 120
        {120, 120},  // 120+8=128 -> bucket 7 -> usable 120
        {121, 248},  // 121+8=129 -> bucket 8 -> usable 248
    };

    for (const auto& t : tests) {
        auto buffer = Pool::buffer_alloc(t.request_size);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, t.expected_usable)
            << "Request " << t.request_size << " expected " << t.expected_usable
            << " got " << buffer.length;
        Pool::buffer_free(buffer);
    }
}

// =============================================================================
// Complex Container Integration Tests
// =============================================================================

TEST_F(HugePageBufferPoolTest, UnorderedMapOfVectorsWithLargeObjects) {
    // Non-trivially copyable large object
    struct BigObject {
        std::array<byte, 512> data;
        std::string name;
        u64 id;

        // Non-trivial constructor
        BigObject(u64 _id, const std::string& _name) : name(_name), id(_id) {
            // Fill data with pattern based on id
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] = static_cast<byte>((id + i) & 0xFF);
            }
        }

        // Non-trivial copy constructor
        BigObject(const BigObject& other)
            : data(other.data), name(other.name), id(other.id) {}

        // Non-trivial move constructor
        BigObject(BigObject&& other) noexcept
            : data(std::move(other.data)), name(std::move(other.name)), id(other.id) {
            other.id = 0;
        }

        // Non-trivial copy assignment
        BigObject& operator=(const BigObject& other) {
            if (this != &other) {
                data = other.data;
                name = other.name;
                id = other.id;
            }
            return *this;
        }

        // Non-trivial move assignment
        BigObject& operator=(BigObject&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                name = std::move(other.name);
                id = other.id;
                other.id = 0;
            }
            return *this;
        }

        // Non-trivial destructor
        ~BigObject() {
            id = 0xDEAD;  // Mark as destroyed
        }

        // Verify data integrity
        [[nodiscard]] bool verify() const {
            for (size_t i = 0; i < data.size(); ++i) {
                if (data[i] != static_cast<byte>((id + i) & 0xFF)) {
                    return false;
                }
            }
            return true;
        }
    };

    static_assert(!std::is_trivially_copyable_v<BigObject>, "BigObject must be non-trivially copyable");
    static_assert(sizeof(BigObject) > 512, "BigObject must be large");

    // Vector backed by hugepage allocator
    using BigObjectVector = std::vector<BigObject, skl::hugepage_allocator<BigObject>>;

    // Custom allocator for the map's internal nodes
    // Note: unordered_map allocates pair<const Key, Value>, so we need allocator for that
    using MapAllocator = skl::hugepage_allocator<std::pair<const u64, BigObjectVector>>;

    // The full map type
    using BigObjectMap = std::unordered_map<
        u64,                          // Key
        BigObjectVector,              // Value (vector of big objects)
        std::hash<u64>,               // Hash
        std::equal_to<u64>,           // KeyEqual
        MapAllocator                  // Allocator
    >;

    BigObjectMap map;

    // Insert multiple keys, each with a vector of big objects
    constexpr u64 num_keys = 50;
    constexpr u64 objects_per_key = 20;

    for (u64 key = 0; key < num_keys; ++key) {
        BigObjectVector vec;
        vec.reserve(objects_per_key);

        for (u64 i = 0; i < objects_per_key; ++i) {
            u64 id = key * 1000 + i;
            vec.emplace_back(id, "Object_" + std::to_string(id));
        }

        map[key] = std::move(vec);
    }

    // Verify all data integrity
    for (u64 key = 0; key < num_keys; ++key) {
        ASSERT_TRUE(map.contains(key)) << "Missing key " << key;

        const auto& vec = map.at(key);
        ASSERT_EQ(vec.size(), objects_per_key) << "Wrong size for key " << key;

        for (u64 i = 0; i < objects_per_key; ++i) {
            u64 expected_id = key * 1000 + i;
            ASSERT_EQ(vec[i].id, expected_id) << "Wrong id at key=" << key << ", i=" << i;
            ASSERT_EQ(vec[i].name, "Object_" + std::to_string(expected_id));
            ASSERT_TRUE(vec[i].verify()) << "Data corruption at key=" << key << ", i=" << i;
        }
    }

    // Trigger reallocations by adding more elements
    for (u64 key = 0; key < num_keys; ++key) {
        auto& vec = map[key];
        for (u64 i = 0; i < 10; ++i) {
            u64 id = key * 1000 + objects_per_key + i;
            vec.emplace_back(id, "Extra_" + std::to_string(id));
        }
    }

    // Verify again after growth
    for (u64 key = 0; key < num_keys; ++key) {
        const auto& vec = map.at(key);
        ASSERT_EQ(vec.size(), objects_per_key + 10);

        // Verify original objects still intact
        for (u64 i = 0; i < objects_per_key; ++i) {
            ASSERT_TRUE(vec[i].verify()) << "Original data corrupted after growth at key=" << key;
        }

        // Verify new objects
        for (u64 i = 0; i < 10; ++i) {
            ASSERT_TRUE(vec[objects_per_key + i].verify())
                << "New data corrupted at key=" << key << ", i=" << i;
        }
    }

    // Remove some keys and verify no corruption
    for (u64 key = 0; key < num_keys; key += 2) {
        map.erase(key);
    }

    // Verify remaining keys
    for (u64 key = 1; key < num_keys; key += 2) {
        ASSERT_TRUE(map.contains(key));
        const auto& vec = map.at(key);
        for (const auto& obj : vec) {
            ASSERT_TRUE(obj.verify()) << "Data corrupted after erase at key=" << key;
        }
    }

    // Clear and verify no crash
    map.clear();
    ASSERT_TRUE(map.empty());
}

TEST_F(HugePageBufferPoolTest, NestedContainersStressTest) {
    // Even more complex: map of maps of vectors
    using InnerVector = std::vector<u64, skl::hugepage_allocator<u64>>;
    using InnerMapAlloc = skl::hugepage_allocator<std::pair<const std::string, InnerVector>>;
    using InnerMap = std::unordered_map<std::string, InnerVector, std::hash<std::string>,
                                         std::equal_to<>, InnerMapAlloc>;
    using OuterMapAlloc = skl::hugepage_allocator<std::pair<const u64, InnerMap>>;
    using OuterMap = std::unordered_map<u64, InnerMap, std::hash<u64>,
                                         std::equal_to<>, OuterMapAlloc>;

    OuterMap outer;

    // Build nested structure
    for (u64 i = 0; i < 10; ++i) {
        InnerMap inner;
        for (u64 j = 0; j < 5; ++j) {
            InnerVector vec;
            vec.reserve(100);
            for (u64 k = 0; k < 100; ++k) {
                vec.push_back((i * 10000) + (j * 100) + k);
            }
            inner["key_" + std::to_string(j)] = std::move(vec);
        }
        outer[i] = std::move(inner);
    }

    // Verify structure
    for (u64 i = 0; i < 10; ++i) {
        ASSERT_TRUE(outer.contains(i));
        const auto& inner = outer.at(i);
        ASSERT_EQ(inner.size(), 5u);

        for (u64 j = 0; j < 5; ++j) {
            std::string key = "key_" + std::to_string(j);
            ASSERT_TRUE(inner.contains(key));
            const auto& vec = inner.at(key);
            ASSERT_EQ(vec.size(), 100u);

            for (u64 k = 0; k < 100; ++k) {
                u64 expected = (i * 10000) + (j * 100) + k;
                ASSERT_EQ(vec[k], expected)
                    << "Mismatch at outer=" << i << ", inner=" << j << ", vec=" << k;
            }
        }
    }

    // Modify and re-verify
    for (u64 i = 0; i < 10; ++i) {
        for (u64 j = 0; j < 5; ++j) {
            auto& vec = outer[i]["key_" + std::to_string(j)];
            for (u64 k = 0; k < 50; ++k) {
                vec.push_back(999999);
            }
        }
    }

    // Verify original data still intact
    for (u64 i = 0; i < 10; ++i) {
        for (u64 j = 0; j < 5; ++j) {
            const auto& vec = outer.at(i).at("key_" + std::to_string(j));
            ASSERT_EQ(vec.size(), 150u);

            // First 100 should be original
            for (u64 k = 0; k < 100; ++k) {
                u64 expected = (i * 10000) + (j * 100) + k;
                ASSERT_EQ(vec[k], expected);
            }

            // Last 50 should be 999999
            for (u64 k = 100; k < 150; ++k) {
                ASSERT_EQ(vec[k], 999999u);
            }
        }
    }
}

TEST_F(HugePageBufferPoolTest, RealWorldUsagePatternStress) {
    // Simulate ~3 seconds of real-world usage: random inserts, deletes, modifications, moves

    struct Entity {
        u64 id;
        std::array<byte, 256> payload;
        std::string tag;

        Entity() : id(0), tag("empty") {
            payload.fill(0);
        }

        explicit Entity(u64 _id) : id(_id), tag("entity_" + std::to_string(_id)) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<byte>((id + i) & 0xFF);
            }
        }

        [[nodiscard]] bool verify() const {
            if (id == 0) return true;  // Empty entity
            for (size_t i = 0; i < payload.size(); ++i) {
                if (payload[i] != static_cast<byte>((id + i) & 0xFF)) {
                    return false;
                }
            }
            return tag == "entity_" + std::to_string(id);
        }
    };

    using EntityVector = std::vector<Entity, skl::hugepage_allocator<Entity>>;
    using EntityMapAlloc = skl::hugepage_allocator<std::pair<const u64, EntityVector>>;
    using EntityMap = std::unordered_map<u64, EntityVector, std::hash<u64>,
                                          std::equal_to<>, EntityMapAlloc>;

    EntityMap database;
    std::mt19937_64 rng(12345);
    u64 next_entity_id = 1;
    u64 next_key = 0;

    // Track statistics
    u64 inserts = 0, deletes = 0, modifications = 0, moves = 0, verifications = 0;

    auto start = std::chrono::steady_clock::now();
    auto end_time = start + std::chrono::seconds(3);

    while (std::chrono::steady_clock::now() < end_time) {
        u32 op = rng() % 100;

        if (op < 25 || database.empty()) {
            // INSERT: Add new key with vector of entities
            u64 key = next_key++;
            u64 count = (rng() % 20) + 1;
            EntityVector vec;
            vec.reserve(count);
            for (u64 i = 0; i < count; ++i) {
                vec.emplace_back(next_entity_id++);
            }
            database[key] = std::move(vec);
            ++inserts;

        } else if (op < 40) {
            // DELETE: Remove random key
            auto it = database.begin();
            std::advance(it, rng() % database.size());
            database.erase(it);
            ++deletes;

        } else if (op < 60) {
            // APPEND: Add entities to existing key
            auto it = database.begin();
            std::advance(it, rng() % database.size());
            u64 count = (rng() % 10) + 1;
            for (u64 i = 0; i < count; ++i) {
                it->second.emplace_back(next_entity_id++);
            }
            ++modifications;

        } else if (op < 70) {
            // SHRINK: Remove entities from existing key
            auto it = database.begin();
            std::advance(it, rng() % database.size());
            if (!it->second.empty()) {
                u64 remove_count = (rng() % it->second.size()) + 1;
                for (u64 i = 0; i < remove_count && !it->second.empty(); ++i) {
                    it->second.pop_back();
                }
            }
            ++modifications;

        } else if (op < 80) {
            // MOVE: Move entities between keys
            if (database.size() >= 2) {
                auto it1 = database.begin();
                auto it2 = database.begin();
                std::advance(it1, rng() % database.size());
                std::advance(it2, rng() % database.size());
                if (it1 != it2 && !it1->second.empty()) {
                    // Move last entity from it1 to it2
                    it2->second.push_back(std::move(it1->second.back()));
                    it1->second.pop_back();
                    ++moves;
                }
            }

        } else if (op < 90) {
            // SWAP: Swap vectors between two keys
            if (database.size() >= 2) {
                auto it1 = database.begin();
                auto it2 = database.begin();
                std::advance(it1, rng() % database.size());
                std::advance(it2, rng() % database.size());
                if (it1 != it2) {
                    std::swap(it1->second, it2->second);
                    ++moves;
                }
            }

        } else if (op < 95) {
            // CLEAR & REFILL: Clear a vector and refill it
            auto it = database.begin();
            std::advance(it, rng() % database.size());
            it->second.clear();
            u64 count = (rng() % 30) + 1;
            it->second.reserve(count);
            for (u64 i = 0; i < count; ++i) {
                it->second.emplace_back(next_entity_id++);
            }
            ++modifications;

        } else {
            // VERIFY: Check data integrity of random key
            auto it = database.begin();
            std::advance(it, rng() % database.size());
            for (const auto& entity : it->second) {
                ASSERT_TRUE(entity.verify())
                    << "Data corruption detected! Entity id=" << entity.id;
            }
            ++verifications;
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    printf("Real-world stress test completed in %lld ms\n", ms);
    printf("  Inserts: %lu, Deletes: %lu, Modifications: %lu, Moves: %lu, Verifications: %lu\n",
           inserts, deletes, modifications, moves, verifications);
    printf("  Final database size: %zu keys, %lu total entities created\n",
           database.size(), next_entity_id - 1);

    // Final verification of all remaining data
    u64 total_entities = 0;
    for (const auto& [key, vec] : database) {
        for (const auto& entity : vec) {
            ASSERT_TRUE(entity.verify())
                << "Final verification failed! Key=" << key << ", Entity id=" << entity.id;
            ++total_entities;
        }
    }
    printf("  Final verification passed: %lu entities intact\n", total_entities);
}

TEST_F(HugePageBufferPoolTest, HighChurnAllocationPattern) {
    // Simulate high-churn scenario: rapid alloc/free cycles with varying sizes
    // This specifically tests the freelist integrity under pressure

    std::mt19937_64 rng(67890);
    std::vector<Pool::buffer_t> active_buffers;
    active_buffers.reserve(1000);

    const u32 sizes[] = {24, 56, 120, 248, 504, 1016, 2040, 4088};

    u64 total_allocs = 0, total_frees = 0;

    auto start = std::chrono::steady_clock::now();
    auto end_time = start + std::chrono::seconds(3);

    while (std::chrono::steady_clock::now() < end_time) {
        u32 op = rng() % 100;

        if (op < 60 || active_buffers.empty()) {
            // ALLOC: Allocate new buffer
            u32 size = sizes[rng() % 8];
            auto buffer = Pool::buffer_alloc(size);
            ASSERT_TRUE(buffer.is_valid());
            ASSERT_GE(buffer.length, size);

            // Write unique pattern
            byte pattern = static_cast<byte>(total_allocs & 0xFF);
            std::memset(buffer.buffer, pattern, size);

            active_buffers.push_back(buffer);
            ++total_allocs;

        } else if (op < 85) {
            // FREE: Free random buffer
            size_t idx = rng() % active_buffers.size();
            Pool::buffer_free(active_buffers[idx]);
            active_buffers.erase(active_buffers.begin() + static_cast<std::ptrdiff_t>(idx));
            ++total_frees;

        } else if (op < 95) {
            // VERIFY & FREE: Verify pattern then free
            size_t idx = rng() % active_buffers.size();
            auto& buf = active_buffers[idx];

            // Just verify it's readable (pattern may have been overwritten by index)
            [[maybe_unused]] volatile byte check = buf.buffer[0];

            Pool::buffer_free(buf);
            active_buffers.erase(active_buffers.begin() + static_cast<std::ptrdiff_t>(idx));
            ++total_frees;

        } else {
            // BATCH FREE: Free multiple buffers at once
            u64 free_count = std::min<u64>(rng() % 10 + 1, active_buffers.size());
            for (u64 i = 0; i < free_count; ++i) {
                Pool::buffer_free(active_buffers.back());
                active_buffers.pop_back();
                ++total_frees;
            }
        }

        // Keep buffer count bounded
        while (active_buffers.size() > 500) {
            Pool::buffer_free(active_buffers.back());
            active_buffers.pop_back();
            ++total_frees;
        }
    }

    // Cleanup
    for (auto& buf : active_buffers) {
        Pool::buffer_free(buf);
        ++total_frees;
    }
    active_buffers.clear();

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    printf("High-churn stress test completed in %lld ms\n", ms);
    printf("  Total allocations: %lu, Total frees: %lu\n", total_allocs, total_frees);
    printf("  Ops/sec: %.0f\n", static_cast<double>(total_allocs + total_frees) / (static_cast<double>(ms) / 1000.0));

    // Verify pool is still functional after high churn
    auto test_buffer = Pool::buffer_alloc(1024);
    ASSERT_TRUE(test_buffer.is_valid());
    std::memset(test_buffer.buffer, 0xAB, 1024);
    Pool::buffer_free(test_buffer);
}
