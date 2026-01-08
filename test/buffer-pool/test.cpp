#include <skl_pool/buffer_pool>
#include <skl_core>

#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <set>
#include <chrono>
#include <cstring>

using Pool = skl::BufferPool;

class BufferPoolTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(skl::skl_core_init().is_success());
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

TEST_F(BufferPoolTest, ConstructDestroy) {
    Pool::destroy_pool();
    ASSERT_EQ(Pool::construct_pool(), SKL_SUCCESS);
}

TEST_F(BufferPoolTest, ConstructWithWarmup) {
    Pool::destroy_pool();
    ASSERT_EQ(Pool::construct_pool(true), SKL_SUCCESS);
}

TEST_F(BufferPoolTest, DestroyIdempotent) {
    Pool::destroy_pool();
    Pool::destroy_pool(); // Should not crash
}

TEST_F(BufferPoolTest, BasicAllocFree) {
    auto buffer = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer.is_valid());
    ASSERT_NE(buffer.buffer, nullptr);
    ASSERT_GE(buffer.length, 64u);

    Pool::buffer_free(buffer.buffer);
}

TEST_F(BufferPoolTest, AllocVariousSizes) {
    const u32 sizes[] = {32, 64, 128, 256, 512, 1024, 4096, 65536};

    for (u32 size : sizes) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid()) << "Failed for size " << size;
        ASSERT_GE(buffer.length, size);

        std::memset(buffer.buffer, 0xAB, buffer.length);

        Pool::buffer_free(buffer.buffer);
    }
}

TEST_F(BufferPoolTest, AllocNonPowerOf2Sizes) {
    const u32 sizes[] = {33, 65, 100, 1000, 5000};

    for (u32 size : sizes) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_GE(buffer.length, size)
            << "Length " << buffer.length << " < requested " << size;

        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Bucket Index Tests
// =============================================================================

TEST_F(BufferPoolTest, BucketIndexEdgeCases) {
    {
        auto result = Pool::buffer_get_pool_index(0);
        ASSERT_TRUE(result.is_success());
        ASSERT_EQ(result.value(), 5u);
    }
    {
        auto result = Pool::buffer_get_pool_index(1);
        ASSERT_TRUE(result.is_success());
        ASSERT_EQ(result.value(), 5u);
    }
}

TEST_F(BufferPoolTest, BucketIndexBoundaries) {
    struct TestCase {
        u32 size;
        u32 expected_bucket;
    };

    const TestCase cases[] = {
        {     32,  5},
        {     33,  6},
        {     64,  6},
        {     65,  7},
        {    128,  7},
        {    256,  8},
        {   1024, 10},
        {   4096, 12},
        {  65536, 16},
        {1 << 20, 20},
        {1 << 21, 21},
    };

    for (const auto& tc : cases) {
        auto result = Pool::buffer_get_pool_index(tc.size);
        ASSERT_TRUE(result.is_success()) << "Failed for size " << tc.size;
        ASSERT_EQ(result.value(), tc.expected_bucket)
            << "Wrong bucket for size " << tc.size;
    }
}

TEST_F(BufferPoolTest, BufferGetSizeForBucket) {
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(5), 32u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(6), 64u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(10), 1024u);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(20), 1u << 20);
    ASSERT_EQ(Pool::buffer_get_size_for_bucket(21), 1u << 21);
}

// =============================================================================
// All Bucket Sizes Tests
// =============================================================================

TEST_F(BufferPoolTest, SmallBuckets5To15) {
    for (u32 bucket = 5; bucket <= 15; ++bucket) {
        u32  size   = (1u << bucket) - 8; // Request usable size
        auto buffer = Pool::buffer_alloc(size);

        ASSERT_TRUE(buffer.is_valid()) << "Failed for bucket " << bucket;
        ASSERT_GE(buffer.length, size) << "Insufficient size for bucket " << bucket;

        std::memset(buffer.buffer, 0xAB, size);
        ASSERT_EQ(buffer.buffer[0], 0xAB);
        ASSERT_EQ(buffer.buffer[size - 1], 0xAB);

        Pool::buffer_free(buffer.buffer);
    }
}

TEST_F(BufferPoolTest, MediumBuckets16To20) {
    for (u32 bucket = 16; bucket <= 20; ++bucket) {
        u32  size   = (1u << bucket) - 8;
        auto buffer = Pool::buffer_alloc(size);

        ASSERT_TRUE(buffer.is_valid()) << "Failed for bucket " << bucket;
        ASSERT_GE(buffer.length, size);

        buffer.buffer[0]        = 0xCD;
        buffer.buffer[size - 1] = 0xCD;
        ASSERT_EQ(buffer.buffer[0], 0xCD);
        ASSERT_EQ(buffer.buffer[size - 1], 0xCD);

        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Multiple Allocation Tests
// =============================================================================

TEST_F(BufferPoolTest, MultipleAllocSameSize) {
    const u32                   count = 100;
    std::vector<Pool::buffer_t> buffers;
    std::set<void*>             addresses;

    for (u32 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(64);
        ASSERT_TRUE(buffer.is_valid());
        buffers.push_back(buffer);
        addresses.insert(buffer.buffer);
    }

    ASSERT_EQ(addresses.size(), count);

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer.buffer);
    }
}

TEST_F(BufferPoolTest, MultipleAllocDifferentSizes) {
    std::vector<Pool::buffer_t> buffers;
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
        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Reuse Tests (LIFO behavior)
// =============================================================================

TEST_F(BufferPoolTest, BufferReuse) {
    auto buffer1 = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer1.is_valid());
    void* addr1 = buffer1.buffer;

    Pool::buffer_free(buffer1.buffer);

    auto buffer2 = Pool::buffer_alloc(64);
    ASSERT_TRUE(buffer2.is_valid());
    void* addr2 = buffer2.buffer;

    ASSERT_EQ(addr1, addr2);

    Pool::buffer_free(buffer2.buffer);
}

TEST_F(BufferPoolTest, LIFOOrder) {
    auto a = Pool::buffer_alloc(64);
    auto b = Pool::buffer_alloc(64);
    auto c = Pool::buffer_alloc(64);

    void* addr_a = a.buffer;
    void* addr_b = b.buffer;
    void* addr_c = c.buffer;

    Pool::buffer_free(c.buffer);
    Pool::buffer_free(b.buffer);
    Pool::buffer_free(a.buffer);

    auto new_a = Pool::buffer_alloc(64);
    auto new_b = Pool::buffer_alloc(64);
    auto new_c = Pool::buffer_alloc(64);

    ASSERT_EQ(new_a.buffer, addr_a);
    ASSERT_EQ(new_b.buffer, addr_b);
    ASSERT_EQ(new_c.buffer, addr_c);

    Pool::buffer_free(new_a.buffer);
    Pool::buffer_free(new_b.buffer);
    Pool::buffer_free(new_c.buffer);
}

// =============================================================================
// Object Allocation Tests
// =============================================================================

TEST_F(BufferPoolTest, ObjectAlloc) {
    struct TestObject {
        u64 a = 0;
        u64 b = 0;
        u64 c = 0;

        TestObject(u64 _a, u64 _b, u64 _c)
            : a(_a), b(_b), c(_c) { }
    };

    auto ptr = Pool::object_alloc<TestObject>(1, 2, 3);
    ASSERT_TRUE(ptr);
    ASSERT_EQ(ptr->a, 1u);
    ASSERT_EQ(ptr->b, 2u);
    ASSERT_EQ(ptr->c, 3u);

    ptr.reset();
}

TEST_F(BufferPoolTest, ObjectAllocDestructorCalled) {
    static int destruct_count = 0;

    struct CountedObject {
        ~CountedObject() { ++destruct_count; }
    };

    destruct_count = 0;

    {
        auto ptr = Pool::object_alloc<CountedObject>();
        ASSERT_TRUE(ptr);
    }

    ASSERT_EQ(destruct_count, 1);
}

TEST_F(BufferPoolTest, ObjectPtrMoveSemantics) {
    struct TestObj {
        int value = 42;
    };

    auto ptr1 = Pool::object_alloc<TestObj>();
    ASSERT_TRUE(ptr1);
    ASSERT_EQ(ptr1->value, 42);

    auto ptr2 = std::move(ptr1);
    ASSERT_FALSE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_EQ(ptr2->value, 42);

    Pool::ptr_t<TestObj> ptr3;
    ptr3 = std::move(ptr2);
    ASSERT_FALSE(ptr2);
    ASSERT_TRUE(ptr3);
    ASSERT_EQ(ptr3->value, 42);
}

TEST_F(BufferPoolTest, ObjectPtrNullptrAssignment) {
    struct TestObj {
        int value = 42;
    };

    auto ptr = Pool::object_alloc<TestObj>();
    ASSERT_TRUE(ptr);

    ptr = nullptr;
    ASSERT_FALSE(ptr);
    ASSERT_EQ(ptr.get(), nullptr);
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(BufferPoolTest, StressRandomAllocFree) {
    std::vector<Pool::buffer_t> allocated;
    std::mt19937                rng(42);
    const u32                   sizes[] = {32, 64, 128, 256, 512, 1024};

    const u64 iterations = 10000;

    for (u64 i = 0; i < iterations; ++i) {
        if (allocated.empty() || (rng() % 2 && allocated.size() < 1000)) {
            u32  size   = sizes[rng() % 6];
            auto buffer = Pool::buffer_alloc(size);
            ASSERT_TRUE(buffer.is_valid());
            allocated.push_back(buffer);
        } else {
            std::uniform_int_distribution<size_t> dist(0, allocated.size() - 1);
            size_t idx = dist(rng);
            Pool::buffer_free(allocated[idx].buffer);
            allocated.erase(allocated.begin() + idx);
        }
    }

    for (auto& buffer : allocated) {
        Pool::buffer_free(buffer.buffer);
    }
}

TEST_F(BufferPoolTest, StressSameSizeAllocFree) {
    const u64 iterations = 50000;

    for (u64 i = 0; i < iterations; ++i) {
        auto buffer = Pool::buffer_alloc(128);
        ASSERT_TRUE(buffer.is_valid());
        Pool::buffer_free(buffer.buffer);
    }
}

TEST_F(BufferPoolTest, LargeScaleAllocation) {
    const u64                   count = 10000;
    std::vector<Pool::buffer_t> buffers;
    buffers.reserve(count);

    for (u64 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(64);
        ASSERT_TRUE(buffer.is_valid());
        buffers.push_back(buffer);
    }

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Memory Pattern Tests
// =============================================================================

TEST_F(BufferPoolTest, WriteReadPattern) {
    auto buffer = Pool::buffer_alloc(4096);
    ASSERT_TRUE(buffer.is_valid());

    for (u32 i = 0; i < buffer.length; ++i) {
        buffer.buffer[i] = static_cast<byte>(i & 0xFF);
    }

    for (u32 i = 0; i < buffer.length; ++i) {
        ASSERT_EQ(buffer.buffer[i], static_cast<byte>(i & 0xFF));
    }

    Pool::buffer_free(buffer.buffer);
}

TEST_F(BufferPoolTest, NoOverlapBetweenAllocations) {
    const u32 size  = 1024;
    const u32 count = 100;

    std::vector<Pool::buffer_t> buffers;

    for (u32 i = 0; i < count; ++i) {
        auto buffer = Pool::buffer_alloc(size);
        ASSERT_TRUE(buffer.is_valid());
        std::memset(buffer.buffer, static_cast<int>(i), size);
        buffers.push_back(buffer);
    }

    for (u32 i = 0; i < count; ++i) {
        for (u32 j = 0; j < size; ++j) {
            ASSERT_EQ(buffers[i].buffer[j], static_cast<byte>(i))
                << "Buffer " << i << " corrupted at offset " << j;
        }
    }

    for (auto& buffer : buffers) {
        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Round to Power of 2 Tests
// =============================================================================

TEST_F(BufferPoolTest, RoundToPowerOf2) {
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
// Header Mechanism Tests
// =============================================================================

TEST_F(BufferPoolTest, HeaderSizeCorrectlyDeducted) {
    // Request 24 bytes -> bucket 5 (32 bytes) -> usable = 24 bytes
    {
        auto buffer = Pool::buffer_alloc(24);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 24u);
        Pool::buffer_free(buffer.buffer);
    }

    // Request 56 bytes -> bucket 6 (64 bytes) -> usable = 56 bytes
    {
        auto buffer = Pool::buffer_alloc(56);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 56u);
        Pool::buffer_free(buffer.buffer);
    }

    // Request 120 bytes -> bucket 7 (128 bytes) -> usable = 120 bytes
    {
        auto buffer = Pool::buffer_alloc(120);
        ASSERT_TRUE(buffer.is_valid());
        ASSERT_EQ(buffer.length, 120u);
        Pool::buffer_free(buffer.buffer);
    }
}

// =============================================================================
// Performance Test
// =============================================================================

TEST_F(BufferPoolTest, PerformanceVsMalloc) {
    const u64 iterations = 100000;
    const u32 size       = 256;

    {
        auto start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            auto buffer = Pool::buffer_alloc(size);
            Pool::buffer_free(buffer.buffer);
        }

        auto pool_time = std::chrono::high_resolution_clock::now() - start;
        printf("Pool alloc/free: %lld ns per cycle\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(pool_time).count() / (long long)iterations);
    }

    {
        auto start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            void* ptr = std::malloc(size);
            std::free(ptr);
        }

        auto malloc_time = std::chrono::high_resolution_clock::now() - start;
        printf("malloc/free: %lld ns per cycle\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(malloc_time).count() / (long long)iterations);
    }
}

// =============================================================================
// Allocator Tests
// =============================================================================

TEST_F(BufferPoolTest, BufferAllocatorBasic) {
    skl::buffer_allocator<int> alloc;

    int* arr = alloc.allocate(10);
    ASSERT_NE(arr, nullptr);

    for (int i = 0; i < 10; ++i) {
        arr[i] = i * 2;
    }

    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(arr[i], i * 2);
    }

    alloc.deallocate(arr, 10);
}

TEST_F(BufferPoolTest, BufferAllocatorWithVector) {
    std::vector<int, skl::buffer_allocator<int>> vec;

    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }

    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(vec[i], i);
    }
}

TEST_F(BufferPoolTest, BufferAllocatorReallocate) {
    skl::buffer_allocator<int> alloc;

    int* arr = alloc.allocate(5);
    for (int i = 0; i < 5; ++i) {
        arr[i] = i + 1;
    }

    int* new_arr = alloc.reallocate(arr, 5, 10);
    ASSERT_NE(new_arr, nullptr);

    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(new_arr[i], i + 1);
    }

    alloc.deallocate(new_arr, 10);
}
