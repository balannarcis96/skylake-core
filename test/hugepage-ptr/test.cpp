//!
//! \file test.cpp
//!
//! \brief Test global ptr_t with complex non-trivial objects backed by hugepage allocator
//!
//! This test verifies that global ptr_t<T> objects with hugepage-backed STL containers
//! are properly cleaned up during static destruction without crashes.
//!
#include <skl_pool/hugepage_buffer_pool>
#include <skl_huge_pages>
#include <skl_core>

#include <gtest/gtest.h>
#include <vector>
#include <unordered_map>

using Pool = skl::HugePageBufferPool;

// =============================================================================
// Complex non-trivial object with hugepage-backed containers
// =============================================================================

//! Entry type with hugepage-backed unordered_map
struct Entry {
    u64 id;
    std::unordered_map<u64, u64, std::hash<u64>, std::equal_to<>,
                       skl::hugepage_allocator<std::pair<const u64, u64>>> data;

    Entry() noexcept = default;

    explicit Entry(u64 f_id) noexcept
        : id(f_id) {
        // Populate the map with some data
        for (u64 i = 0; i < 10; ++i) {
            data[i] = (f_id * 100) + i;
        }
    }

    ~Entry() noexcept = default;

    Entry(const Entry&)            = default;
    Entry& operator=(const Entry&) = default;
    Entry(Entry&&)                 = default;
    Entry& operator=(Entry&&)      = default;
};

//! Non-trivial object containing hugepage-backed vector of entries
struct ComplexObject {
    u64 object_id;
    std::vector<Entry, skl::hugepage_allocator<Entry>> entries;

    ComplexObject() noexcept = default;

    explicit ComplexObject(u64 f_id, u64 f_entry_count) noexcept
        : object_id(f_id) {
        // Populate with entries
        entries.reserve(f_entry_count);
        for (u64 i = 0; i < f_entry_count; ++i) {
            entries.emplace_back((f_id * 1000) + i);
        }
    }

    ~ComplexObject() noexcept = default;

    ComplexObject(const ComplexObject&)            = delete;
    ComplexObject& operator=(const ComplexObject&) = delete;
    ComplexObject(ComplexObject&&)                 = default;
    ComplexObject& operator=(ComplexObject&&)      = default;
};

// =============================================================================
// Global ptr_t instance - will be destroyed during static destruction
// =============================================================================
Pool::ptr_t<ComplexObject> g_global_complex_object{nullptr};

// =============================================================================
// Test fixture
// =============================================================================
class HugePagePtrGlobalTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(skl::skl_core_init().is_success());
    }

    static void TearDownTestSuite() {
        // Must free global ptr BEFORE skl_core_deinit() destroys the pool.
        // Static destruction order is not guaranteed, so caller is responsible
        // for freeing all hugepage allocations before deinit.
        g_global_complex_object = nullptr;
        ASSERT_TRUE(skl::skl_core_deinit().is_success());
    }
};

// =============================================================================
// Tests
// =============================================================================

//! Test that global ptr_t with complex object can be allocated and accessed
TEST_F(HugePagePtrGlobalTest, GlobalPtrAllocation) {
    // Allocate complex object into global ptr
    g_global_complex_object = Pool::object_alloc<ComplexObject>(42u, 100u);

    ASSERT_TRUE(g_global_complex_object != nullptr);
    ASSERT_NE(nullptr, g_global_complex_object); // Test nullptr on left side
    ASSERT_EQ(g_global_complex_object->object_id, 42u);
    ASSERT_EQ(g_global_complex_object->entries.size(), 100u);

    // Verify data integrity
    for (u64 i = 0; i < g_global_complex_object->entries.size(); ++i) {
        const auto& entry = g_global_complex_object->entries[i];
        ASSERT_EQ(entry.id, (42ull * 1000) + i);
        ASSERT_EQ(entry.data.size(), 10u);

        for (u64 j = 0; j < 10; ++j) {
            auto it = entry.data.find(j);
            ASSERT_NE(it, entry.data.end());
            ASSERT_EQ(it->second, (entry.id * 100) + j);
        }
    }
}

//! Test that local ptr_t with complex object works correctly
TEST_F(HugePagePtrGlobalTest, LocalPtrAllocation) {
    auto local_ptr = Pool::object_alloc<ComplexObject>(99u, 50u);

    ASSERT_TRUE(local_ptr != nullptr);
    ASSERT_EQ(local_ptr->object_id, 99u);
    ASSERT_EQ(local_ptr->entries.size(), 50u);

    // Verify entries
    for (u64 i = 0; i < local_ptr->entries.size(); ++i) {
        const auto& entry = local_ptr->entries[i];
        ASSERT_EQ(entry.id, (99ull * 1000) + i);
        ASSERT_EQ(entry.data.size(), 10u);
    }

    // local_ptr goes out of scope here - should not crash
}

//! Test ptr_t move semantics with complex objects
TEST_F(HugePagePtrGlobalTest, PtrMoveSemantics) {
    auto ptr1 = Pool::object_alloc<ComplexObject>(1u, 10u);
    ASSERT_TRUE(ptr1 != nullptr);

    // Move to ptr2
    auto ptr2 = std::move(ptr1);
    ASSERT_TRUE(ptr2 != nullptr);
    ASSERT_TRUE(ptr1 == nullptr); // NOLINT - testing moved-from state

    ASSERT_EQ(ptr2->object_id, 1u);
    ASSERT_EQ(ptr2->entries.size(), 10u);

    // Move assign
    Pool::ptr_t<ComplexObject> ptr3{nullptr};
    ptr3 = std::move(ptr2);
    ASSERT_TRUE(ptr3 != nullptr);
    ASSERT_TRUE(ptr2 == nullptr); // NOLINT - testing moved-from state
}

//! Test ptr_t swap operation
TEST_F(HugePagePtrGlobalTest, PtrSwap) {
    auto ptr1 = Pool::object_alloc<ComplexObject>(111u, 5u);
    auto ptr2 = Pool::object_alloc<ComplexObject>(222u, 7u);

    ASSERT_EQ(ptr1->object_id, 111u);
    ASSERT_EQ(ptr2->object_id, 222u);

    ptr1.swap(ptr2);

    ASSERT_EQ(ptr1->object_id, 222u);
    ASSERT_EQ(ptr2->object_id, 111u);
    ASSERT_EQ(ptr1->entries.size(), 7u);
    ASSERT_EQ(ptr2->entries.size(), 5u);
}

//! Test multiple allocations and deallocations
TEST_F(HugePagePtrGlobalTest, MultipleAllocDealloc) {
    std::vector<Pool::ptr_t<ComplexObject>> objects;
    objects.reserve(10);

    // Allocate many objects
    for (u64 i = 0; i < 10; ++i) {
        objects.push_back(Pool::object_alloc<ComplexObject>(i, 20u));
        ASSERT_TRUE(objects.back() != nullptr);
    }

    // Verify all objects
    for (u64 i = 0; i < 10; ++i) {
        ASSERT_EQ(objects[i]->object_id, i);
        ASSERT_EQ(objects[i]->entries.size(), 20u);
    }

    // Clear - should deallocate all without crash
    objects.clear();
}

//! Test nullptr assignment and comparison operators
TEST_F(HugePagePtrGlobalTest, NullptrOperations) {
    Pool::ptr_t<ComplexObject> ptr{nullptr};

    ASSERT_TRUE(ptr == nullptr);
    ASSERT_TRUE(nullptr == ptr);
    ASSERT_FALSE(ptr != nullptr);
    ASSERT_FALSE(nullptr != ptr);
    ASSERT_FALSE(static_cast<bool>(ptr));

    ptr = Pool::object_alloc<ComplexObject>(1u, 1u);

    ASSERT_FALSE(ptr == nullptr);
    ASSERT_FALSE(nullptr == ptr);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(nullptr != ptr);
    ASSERT_TRUE(static_cast<bool>(ptr));

    ptr = nullptr;

    ASSERT_TRUE(ptr == nullptr);
    ASSERT_TRUE(nullptr == ptr);
}

//! Test release operation
TEST_F(HugePagePtrGlobalTest, ReleaseOperation) {
    auto ptr = Pool::object_alloc<ComplexObject>(777u, 3u);
    ASSERT_TRUE(ptr != nullptr);

    ComplexObject* raw = ptr.release();
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_NE(raw, nullptr);
    ASSERT_EQ(raw->object_id, 777u);

    // Manually free the released pointer
    Pool::object_free(raw);
}

//! Test reset with new pointer
TEST_F(HugePagePtrGlobalTest, ResetWithNewPointer) {
    auto ptr = Pool::object_alloc<ComplexObject>(1u, 1u);
    ASSERT_EQ(ptr->object_id, 1u);

    // Allocate new object and reset
    auto new_alloc = Pool::object_alloc<ComplexObject>(2u, 2u);
    ComplexObject* raw = new_alloc.release();

    ptr.reset(raw); // Old object freed, new object owned

    ASSERT_EQ(ptr->object_id, 2u);
    ASSERT_EQ(ptr->entries.size(), 2u);
}
