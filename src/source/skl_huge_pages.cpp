//!
//! \file skl_huge_pages
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <sys/mman.h>
#include <unistd.h>

#include "skl_huge_pages"
#include "skl_assert"
#include "skl_vector"

namespace {
//! [Internal] Global flag indicating if huge pages are available
bool g_huge_pages_available = false;

//! [Internal] Cached system page size
u64 g_sys_page_size = 0u;

//! [Internal] Pin pages by touching them to force kernel allocation
//!
//! \param f_ptr Pointer to memory region
//! \param f_page_count Number of 2MB pages to pin
//!
//! \remark Kernel postpones actual physical memory allocation until first page fault
//! \remark This function forces immediate allocation by writing to each page
inline void pin_huge_pages(void* f_ptr, u64 f_page_count) noexcept {
    auto* byte_ptr = static_cast<volatile u8*>(f_ptr);

    // Touch the first byte of each 2MB page to trigger page fault and pin it
    for (u64 i = 0u; i < f_page_count; ++i) {
        const u64 page_offset = i * skl::huge_pages::CHugePageSize;
        byte_ptr[page_offset] = 0u;
    }
}
} // namespace

namespace skl::huge_pages {
bool skl_huge_pages_init() noexcept {
    // Get system page size
    const long page_size = ::sysconf(_SC_PAGESIZE);
    SKL_ASSERT_PERMANENT(page_size > 0);
    g_sys_page_size = static_cast<u64>(page_size);

    // Try to allocate a single 2MB huge page as a test
    void* test_ptr = ::mmap(nullptr,
                            CHugePageSize,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                            -1,
                            0);

    if (test_ptr == MAP_FAILED) {
        g_huge_pages_available = false;
        return false;
    }

    // Free the test allocation (no need to pin, we're just testing availability)
    ::munmap(test_ptr, CHugePageSize);

    g_huge_pages_available = true;
    return true;
}

bool is_huge_pages_enabled() noexcept {
    return g_huge_pages_available;
}

void* skl_huge_page_alloc(u64 f_page_count) noexcept {
#if defined(SKL_CORE_FORCE_HUGEPAGE_SUPPORT)
    SKL_ASSERT_PERMANENT(g_huge_pages_available && "Huge pages are not available");
    SKL_ASSERT_PERMANENT(f_page_count > 0u);

    const u64 total_size = f_page_count * CHugePageSize;

    void* ptr = ::mmap(nullptr,
                       total_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                       -1,
                       0);

    SKL_ASSERT_PERMANENT(ptr != MAP_FAILED);

    // Pin pages to force immediate physical memory allocation
    pin_huge_pages(ptr, f_page_count);

    return ptr;
#else
    return skl_huge_page_alloc_or_fallback(f_page_count);
#endif
}

void skl_huge_page_free(void* f_ptr, u64 f_page_count) noexcept {
#if defined(SKL_CORE_FORCE_HUGEPAGE_SUPPORT)
    if (nullptr == f_ptr) {
        return;
    }

    SKL_ASSERT_PERMANENT(f_page_count > 0u);

    const u64 total_size = f_page_count * CHugePageSize;

    [[maybe_unused]] const int result = ::munmap(f_ptr, total_size);
    SKL_ASSERT_PERMANENT(0 == result);
#else
    skl_huge_page_free_or_fallback(f_ptr, f_page_count);
#endif
}

void* skl_huge_page_alloc_or_fallback(u64 f_page_count) noexcept {
    SKL_ASSERT_PERMANENT(f_page_count > 0u);

    if (is_huge_pages_enabled()) {
        const u64 total_size = f_page_count * CHugePageSize;

        void* ptr = ::mmap(nullptr,
                           total_size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                           -1,
                           0);

        SKL_ASSERT_PERMANENT(ptr != MAP_FAILED);

        // Pin pages to force immediate physical memory allocation
        pin_huge_pages(ptr, f_page_count);

        return ptr;
    }

    const u64 total_bytes = page_count_to_bytes(f_page_count);
    return skl_vector_alloc(total_bytes, SKL_CACHE_LINE_SIZE);
}

void skl_huge_page_free_or_fallback(void* f_ptr, u64 f_page_count) noexcept {
    if (nullptr == f_ptr) {
        return;
    }

    if (is_huge_pages_enabled()) {
        SKL_ASSERT_PERMANENT(f_page_count > 0u);

        const u64 total_size = f_page_count * CHugePageSize;

        [[maybe_unused]] const int result = ::munmap(f_ptr, total_size);
        SKL_ASSERT_PERMANENT(0 == result);
    } else {
        skl_vector_free(f_ptr);
    }
}

u64 get_system_page_size() noexcept {
    return g_sys_page_size;
}
} // namespace skl::huge_pages
