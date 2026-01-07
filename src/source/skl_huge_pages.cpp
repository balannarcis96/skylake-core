//!
//! \file skl_huge_pages
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>

#include "skl_huge_pages"
#include "skl_assert"
#include "skl_vector"
#include "skl_core"

namespace {
//! [Internal] Global flag indicating if huge pages are available
bool g_huge_pages_available = false;

//! [Internal] Cached system page size
u64 g_sys_page_size = 0u;

//! [Internal] Cached system huge page size
u64 g_sys_huge_page_size = 0u;

//! [Internal] Read the system's default huge page size from /proc/meminfo
//! \return The huge page size in bytes, or 0 if not available
u64 read_system_huge_page_size() noexcept {
    FILE* file = ::fopen("/proc/meminfo", "r");
    if (nullptr == file) {
        return 0u;
    }

    char line[256];
    u64  hugepage_size_kb = 0u;

    while (::fgets(line, sizeof(line), file) != nullptr) {
        // Look for "Hugepagesize:" line (e.g., "Hugepagesize:       2048 kB")
        if (::sscanf(line, "Hugepagesize: %lu kB", &hugepage_size_kb) == 1) {
            break;
        }
    }

    ::fclose(file);

    // Convert from kB to bytes
    return hugepage_size_kb * 1024u;
}
} // namespace

namespace skl::huge_pages {
namespace internal {
    bool skl_huge_pages_init() noexcept {
        // Get system page size
        const long page_size = ::sysconf(_SC_PAGESIZE);
        SKL_ASSERT_PERMANENT(page_size > 0);
        g_sys_page_size = static_cast<u64>(page_size);

        // Get system huge page size
        g_sys_huge_page_size = read_system_huge_page_size();

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

        // Hugepages are available - verify size matches our expected constant
        SKL_ASSERT_PERMANENT(g_sys_huge_page_size == CHugePageSize
                             && "System huge page size does not match CHugePageSize (expected 2MB)");

        g_huge_pages_available = true;
        return true;
    }
} // namespace internal

u64 get_system_huge_page_size() noexcept {
    return g_sys_huge_page_size;
}

bool is_huge_pages_enabled() noexcept {
    return g_huge_pages_available;
}

void* skl_huge_page_alloc(u64 f_page_count) noexcept {
    SKL_ASSERT_CRITICAL(skl_core_is_initialized());

#if defined(SKL_CORE_FORCE_HUGEPAGE_SUPPORT)
    SKL_ASSERT_PERMANENT(g_huge_pages_available && "Huge pages are not available");
    SKL_ASSERT_PERMANENT(f_page_count > 0u);

    const u64 total_size = f_page_count * CHugePageSize;

    void* ptr = ::mmap(nullptr,
                       total_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE,
                       -1,
                       0);

    SKL_ASSERT_PERMANENT(ptr != MAP_FAILED);

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
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE,
                           -1,
                           0);

        SKL_ASSERT_PERMANENT(ptr != MAP_FAILED);

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
