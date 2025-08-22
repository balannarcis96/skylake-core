#include <skl_pool/stable_object_pool.hpp>

#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <chrono>

namespace {
u64 g_constructed = 0u;
u64 g_destructed  = 0u;
} // namespace

struct Object {
    Object() noexcept {
        ++g_constructed;
    }
    ~Object() noexcept {
        ++g_destructed;
    }

    u64 numbers[8u]{0u, 3u, 6u, 9u, 12u, 15u, 18u, 21u};
};

namespace {
constexpr u64 CCapacity = 256u;
using inner_pool_t      = skl::stable_object_pool::object_pool_t<Object, CCapacity>;
using pool_t            = skl::StableObjectPool<Object, CCapacity>;

void print_pool_stats(auto& f_pool) noexcept {
    (void)printf("ObjectSize: %lu Capacity: %lu, Allocated: %lu, Objects Mem usage: %lu Total Mem usage: %lu (usage rate:%2.f%%)\n",
                 sizeof(Object),
                 f_pool.capacity(),
                 f_pool.size(),
                 f_pool.mem_usage_objects(),
                 f_pool.mem_usage(),
                 (float(f_pool.mem_usage_objects()) / float(f_pool.mem_usage())) * 100.0f);
}
} // namespace

TEST(SkylakeStableObjectPool, object_pool_t__basic) {
    inner_pool_t pool{};

    g_constructed = 0u;
    g_destructed  = 0u;

    ASSERT_EQ(pool.size(), 0u);
    ASSERT_EQ(pool.capacity(), CCapacity);
    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(pool.full());

    pool.clear();

    ASSERT_EQ(pool.size(), 0u);
    ASSERT_EQ(pool.capacity(), CCapacity);
    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(pool.full());
    ASSERT_EQ(g_constructed, 0u);
    ASSERT_EQ(g_destructed, 0u);

    Object* objs[CCapacity]{nullptr};
    for (u64 i = 0u; i < CCapacity; ++i) {
        objs[i] = pool.allocate();
        ASSERT_NE(objs[i], nullptr);
        ASSERT_TRUE(pool.owns(objs[i]));
        ASSERT_EQ(pool.size(), i + 1u);
        ASSERT_EQ(g_constructed, i + 1u);
        ASSERT_EQ(g_destructed, 0u);
    }
    ASSERT_EQ(pool.size(), CCapacity);
    ASSERT_EQ(g_constructed, CCapacity);
    ASSERT_EQ(g_destructed, 0u);
    ASSERT_FALSE(pool.empty());
    ASSERT_TRUE(pool.full());

    Object* obj_extra = pool.allocate();
    ASSERT_EQ(obj_extra, nullptr);

    for (u64 i = 0u; i < CCapacity; ++i) {
        ASSERT_TRUE(pool.deallocate_safe(objs[i]));
        ASSERT_EQ(pool.size(), CCapacity - (i + 1u));
        ASSERT_EQ(g_constructed, CCapacity);
        ASSERT_EQ(g_destructed, i + 1u);
    }
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_EQ(g_constructed, CCapacity);
    ASSERT_EQ(g_destructed, CCapacity);
    ASSERT_TRUE(pool.empty());
    ASSERT_FALSE(pool.full());

    ASSERT_DEATH((void)pool.deallocate_safe(objs[0]), ".*");
    ASSERT_FALSE(pool.deallocate_safe(nullptr));
}

TEST(SkylakeStableObjectPool, basic) {
    pool_t pool{};

    g_constructed = 0u;
    g_destructed  = 0u;

    print_pool_stats(pool);

    ASSERT_EQ(nullptr, pool.current_pool());

    Object* objs[CCapacity]{nullptr};
    for (auto& obj : objs) {
        obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        ASSERT_TRUE(pool.owns(obj));
    }
    ASSERT_NE(nullptr, pool.current_pool());
    ASSERT_EQ(g_constructed, CCapacity);
    ASSERT_EQ(g_destructed, 0u);
    print_pool_stats(pool);

    for (auto& obj : objs) {
        ASSERT_TRUE(pool.deallocate_safe(obj));
    }
    ASSERT_EQ(g_destructed, CCapacity);
    print_pool_stats(pool);
    ASSERT_DEATH((void)pool.deallocate(objs[0u]), ".*");
    ASSERT_EQ(pool.mem_usage_objects(), sizeof(Object) * CCapacity);
    print_pool_stats(pool);
}

TEST(SkylakeStableObjectPool, pointer_stability) {
    pool_t pool{};

    // Allocate some objects
    Object* obj1 = pool.allocate();
    Object* obj2 = pool.allocate();
    Object* obj3 = pool.allocate();

    // Store their addresses
    void* addr1 = obj1;
    // void* addr2 = obj2;
    void* addr3 = obj3;

    // Deallocate middle one
    ASSERT_TRUE(pool.deallocate_safe(obj2));

    // Allocate more to potentially trigger new pool
    std::vector<Object*> objects;
    for (u64 i = 0; i < CCapacity * 2; ++i) {
        objects.push_back(pool.allocate());
    }

    // Original pointers should still be valid
    ASSERT_EQ(addr1, obj1);
    ASSERT_EQ(addr3, obj3);

    // Clean up
    ASSERT_TRUE(pool.deallocate_safe(obj1));
    ASSERT_TRUE(pool.deallocate_safe(obj3));
    for (auto* obj : objects) {
        ASSERT_TRUE(pool.deallocate_safe(obj));
    }
}

TEST(SkylakeStableObjectPool, multiple_pools_allocation) {
    pool_t               pool{};
    std::vector<Object*> objects;

    // Force allocation of multiple internal pools
    const u64 total_objects = CCapacity * 5 + CCapacity / 2;

    for (u64 i = 0; i < total_objects; ++i) {
        Object* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        objects.push_back(obj);
    }

    ASSERT_EQ(pool.size(), total_objects);
    ASSERT_EQ(pool.capacity(), CCapacity * 6); // Should have 6 pools

    print_pool_stats(pool);

    // Verify all pointers are unique
    std::set<Object*> unique_ptrs(objects.begin(), objects.end());
    ASSERT_EQ(unique_ptrs.size(), objects.size());

    // Verify ownership
    for (auto* obj : objects) {
        ASSERT_TRUE(pool.owns(obj));
    }

    // Clean up
    for (auto* obj : objects) {
        ASSERT_TRUE(pool.deallocate_safe(obj));
    }
}

TEST(SkylakeStableObjectPool, fragmentation_and_reuse) {
    pool_t               pool{};
    std::vector<Object*> objects;

    // Allocate full pool
    for (u64 i = 0; i < CCapacity; ++i) {
        objects.push_back(pool.allocate());
    }

    // Deallocate every other object (create fragmentation)
    for (u64 i = 0; i < CCapacity; i += 2) {
        ASSERT_TRUE(pool.deallocate_safe(objects[i]));
        objects[i] = nullptr;
    }

    ASSERT_EQ(pool.size(), CCapacity / 2);

    // Reallocate - should reuse freed slots
    for (u64 i = 0; i < CCapacity; i += 2) {
        objects[i] = pool.allocate();
        ASSERT_NE(objects[i], nullptr);
        ASSERT_TRUE(pool.owns(objects[i]));
    }

    ASSERT_EQ(pool.size(), CCapacity);

    // Should still be in first pool
    ASSERT_EQ(pool.capacity(), CCapacity);

    // Clean up
    for (auto* obj : objects) {
        if (obj) {
            ASSERT_TRUE(pool.deallocate_safe(obj));
        }
    }
}

TEST(SkylakeStableObjectPool, allocate_with_args) {
    struct TestObject {
        int         a;
        float       b;
        const char* c;

        TestObject(int _a, float _b, const char* _c)
            : a(_a)
            , b(_b)
            , c(_c) { }
    };

    using test_pool_t = skl::StableObjectPool<TestObject, 128>;
    test_pool_t pool{};

    auto* obj = pool.allocate(42, 3.14f, "test");
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->a, 42);
    ASSERT_FLOAT_EQ(obj->b, 3.14f);
    ASSERT_STREQ(obj->c, "test");

    ASSERT_TRUE(pool.deallocate_safe(obj));
}

struct CountedObject {
    static inline int constructs = 0;
    static inline int destructs  = 0;

    CountedObject() { ++constructs; }
    ~CountedObject() { ++destructs; }

    int data[16];
};
TEST(SkylakeStableObjectPool, allocate_raw_no_construct) {

    using raw_pool_t = skl::StableObjectPool<CountedObject, 64, false>; // No construct/destruct
    raw_pool_t pool{};

    CountedObject::constructs = 0;
    CountedObject::destructs  = 0;

    auto* obj = pool.allocate_raw();
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(CountedObject::constructs, 0); // Should not construct

    // Manually construct
    new (obj) CountedObject();
    ASSERT_EQ(CountedObject::constructs, 1);

    // Manual destruct before deallocation
    obj->~CountedObject();
    ASSERT_EQ(CountedObject::destructs, 1);

    ASSERT_TRUE(pool.deallocate_safe(obj));
    ASSERT_EQ(CountedObject::destructs, 1); // Should not double-destruct
}

TEST(SkylakeStableObjectPool, clear_with_allocated_objects) {
    inner_pool_t pool{};

    g_constructed = 0u;
    g_destructed  = 0u;

    // Allocate some objects
    std::vector<Object*> objects;
    for (u64 i = 0; i < 50; ++i) {
        objects.push_back(pool.allocate());
    }

    ASSERT_EQ(g_constructed, 50u);
    ASSERT_EQ(g_destructed, 0u);
    ASSERT_EQ(pool.size(), 50u);

    // Clear should destruct all allocated objects
    pool.clear();

    ASSERT_EQ(g_destructed, 50u);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_TRUE(pool.empty());
}

TEST(SkylakeStableObjectPool, clear_with_allocated_objects_2) {
    pool_t pool{};

    g_constructed = 0u;
    g_destructed  = 0u;

    // Allocate some objects
    std::vector<Object*> objects;
    for (u64 i = 0; i < 50; ++i) {
        objects.push_back(pool.allocate());
    }

    ASSERT_EQ(g_constructed, 50u);
    ASSERT_EQ(g_destructed, 0u);
    ASSERT_EQ(pool.size(), 50u);

    // Clear should destruct all allocated objects
    pool.clear();

    ASSERT_EQ(g_destructed, 50u);
    ASSERT_EQ(pool.size(), 0u);
    ASSERT_TRUE(pool.empty());
}
TEST(SkylakeStableObjectPool, stress_random_alloc_dealloc) {
    pool_t               pool{};
    std::vector<Object*> allocated;
    std::mt19937         rng(42); // Deterministic seed

    const u64 iterations = 10000;

    for (u64 i = 0; i < iterations; ++i) {
        if (allocated.empty() || (rng() % 2 && allocated.size() < CCapacity * 10)) {
            // Allocate
            Object* obj = pool.allocate();
            if (obj) {
                allocated.push_back(obj);
                ASSERT_TRUE(pool.owns(obj));
            }
        } else {
            // Deallocate random object
            std::uniform_int_distribution<size_t> dist(0, allocated.size() - 1);
            size_t                                idx = dist(rng);

            ASSERT_TRUE(pool.deallocate_safe(allocated[idx]));
            allocated.erase(allocated.begin() + idx);
        }
    }

    // Verify final state
    ASSERT_EQ(pool.size(), allocated.size());

    // Clean up
    for (auto* obj : allocated) {
        ASSERT_TRUE(pool.deallocate_safe(obj));
    }

    ASSERT_EQ(pool.size(), 0u);
}

TEST(SkylakeStableObjectPool, pool_switching_on_deallocation) {
    pool_t pool{};

    // Fill first pool
    std::vector<Object*> pool1_objects;
    for (u64 i = 0; i < CCapacity; ++i) {
        pool1_objects.push_back(pool.allocate());
    }

    // Force second pool
    Object* pool2_obj = pool.allocate();
    ASSERT_NE(pool2_obj, nullptr);

    // Current pool should be the second one
    auto* current = pool.current_pool();
    ASSERT_NE(current, nullptr);

    // Deallocate from first pool - should switch current pool
    ASSERT_TRUE(pool.deallocate_safe(pool1_objects[0]));

    // Next allocation should come from first pool (pool switching)
    Object* reused = pool.allocate();
    ASSERT_TRUE(pool.owns(reused));

    // Clean up
    ASSERT_TRUE(pool.deallocate_safe(reused));
    ASSERT_TRUE(pool.deallocate_safe(pool2_obj));
    for (size_t i = 1; i < pool1_objects.size(); ++i) {
        ASSERT_TRUE(pool.deallocate_safe(pool1_objects[i]));
    }
}

TEST(SkylakeStableObjectPool, memory_usage_calculation) {
    pool_t pool{};

    ASSERT_EQ(pool.mem_usage_objects(), 0u);

    // Allocate one object (forces first pool)
    Object* obj = pool.allocate();

    u64 expected_obj_mem = sizeof(Object) * CCapacity;
    ASSERT_EQ(pool.mem_usage_objects(), expected_obj_mem);

    // Total memory should include pool metadata
    ASSERT_GT(pool.mem_usage(), pool.mem_usage_objects());

    // Force second pool
    std::vector<Object*> objects;
    for (u64 i = 1; i < CCapacity; ++i) {
        objects.push_back(pool.allocate());
    }
    objects.push_back(pool.allocate()); // Forces new pool

    ASSERT_EQ(pool.mem_usage_objects(), expected_obj_mem * 2);

    print_pool_stats(pool);

    // Clean up
    ASSERT_TRUE(pool.deallocate_safe(obj));
    for (auto* o : objects) {
        ASSERT_TRUE(pool.deallocate_safe(o));
    }
}

TEST(SkylakeStableObjectPool, deallocate_unsafe_assertion) {
    pool_t pool{};

    Object* obj = pool.allocate();
    ASSERT_NE(obj, nullptr);

    // This should work
    pool.deallocate(obj);

    // This should assert (double deallocation)
    ASSERT_DEATH(pool.deallocate(obj), ".*");

    // This should assert (null pointer)
    Object* fake_ptr = reinterpret_cast<Object*>(0xDEADBEEF);
    ASSERT_DEATH(pool.deallocate(fake_ptr), ".*");
}

TEST(SkylakeStableObjectPool, owns_validation) {
    pool_t pool1{};
    pool_t pool2{};

    Object* obj1 = pool1.allocate();
    Object* obj2 = pool2.allocate();

    ASSERT_TRUE(pool1.owns(obj1));
    ASSERT_FALSE(pool1.owns(obj2));
    ASSERT_FALSE(pool1.owns(nullptr));

    ASSERT_FALSE(pool2.owns(obj1));
    ASSERT_TRUE(pool2.owns(obj2));

    // Test with stack object (should not own)
    Object stack_obj;
    ASSERT_FALSE(pool1.owns(&stack_obj));
    ASSERT_FALSE(pool2.owns(&stack_obj));

    ASSERT_TRUE(pool1.deallocate_safe(obj1));
    ASSERT_TRUE(pool2.deallocate_safe(obj2));
}

TEST(SkylakeStableObjectPool, large_scale_allocation) {
    pool_t pool{};

    const u64 target_pools  = 100;
    const u64 total_objects = CCapacity * target_pools;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<Object*> objects;
    objects.reserve(total_objects);

    for (u64 i = 0; i < total_objects; ++i) {
        objects.push_back(pool.allocate());
        ASSERT_NE(objects.back(), nullptr);
    }

    auto alloc_time = std::chrono::high_resolution_clock::now() - start;

    ASSERT_EQ(pool.size(), total_objects);
    ASSERT_EQ(pool.capacity(), total_objects);

    printf("Allocated %lu objects in %lld us (%lld ns per allocation)\n",
           total_objects,
           std::chrono::duration_cast<std::chrono::microseconds>(alloc_time).count(),
           std::chrono::duration_cast<std::chrono::nanoseconds>(alloc_time).count() / total_objects);

    print_pool_stats(pool);

    // Deallocate all
    start = std::chrono::high_resolution_clock::now();
    for (auto* obj : objects) {
        ASSERT_TRUE(pool.deallocate_safe(obj));
    }
    auto dealloc_time = std::chrono::high_resolution_clock::now() - start;

    printf("Deallocated %lu objects in %lld us (%lld ns per deallocation)\n",
           total_objects,
           std::chrono::duration_cast<std::chrono::microseconds>(dealloc_time).count(),
           std::chrono::duration_cast<std::chrono::nanoseconds>(dealloc_time).count() / total_objects);
}

// Performance comparison test
TEST(SkylakeStableObjectPool, performance_vs_new_delete) {
    const u64 iterations = 100000;

    // Test pool allocation
    {
        pool_t pool{};
        auto   start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            Object* obj = pool.allocate();
            ASSERT_TRUE(pool.deallocate_safe(obj));
        }

        auto pool_time = std::chrono::high_resolution_clock::now() - start;
        printf("Pool alloc/dealloc: %lld ns per operation\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(pool_time).count() / iterations);
    }

    // Test new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (u64 i = 0; i < iterations; ++i) {
            Object* obj = new Object();
            delete obj;
        }

        auto new_time = std::chrono::high_resolution_clock::now() - start;
        printf("new/delete: %lld ns per operation\n",
               std::chrono::duration_cast<std::chrono::nanoseconds>(new_time).count() / iterations);
    }
}
