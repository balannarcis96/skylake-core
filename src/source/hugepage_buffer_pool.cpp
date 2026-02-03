#include "skl_pool/hugepage_buffer_pool"
#include "skl_huge_pages"
#include "skl_fixed_vector_if"

#if !SKL_BUILD_SHIPPING
#    include <unordered_set>
#    include <cstdint>
#endif

namespace {
//! Intrusive freelist node - embedded in free buffers (8 bytes)
struct free_node_t {
    free_node_t* next;
};

//! Buffer header - stored at start of allocated buffer (8 bytes)
//! User pointer is rebased past this header
struct buffer_header_t {
    u32 allocated_size; //!< Actual allocated size (power of 2, includes header)
#if !SKL_BUILD_SHIPPING
    u32 magic; //!< Debug magic number for validation
#else
    u32 _padding; //!< Keep 8-byte alignment in shipping
#endif
};
static_assert(sizeof(buffer_header_t) == 8, "Header must be 8 bytes for alignment");

//! Magic number for debug validation
[[maybe_unused]] constexpr u32 CBufferMagic = 0xB0FFE42D;

//! Tracking entry for allocated huge pages
struct hugepage_ptr_t {
    void* ptr;
    u32   page_count;
};

//! Pool metadata - fits in one huge page
struct metadata_t {
    //! Head pointers for each bucket's intrusive freelist (indices 5-27 used)
    free_node_t* bucket_heads[28u] = {};

    //! Tracking of all allocated huge pages (buffer pages only, no freelist overhead)
    //! 65,536 entries = 128GB max capacity
    skl::skl_fixed_vector<hugepage_ptr_t, 1 << 16u> allpages;
};
static_assert(sizeof(metadata_t) <= skl::huge_pages::CHugePageSize, "Metadata size exceeds huge page size");
} // namespace

namespace {
constexpr u8                  CMinBucketIndex = 5u;                      //!< Minimum bucket index (5 for 32-byte buffers)
constexpr u8                  CMaxBucketIndex = 27u;                     //!< Maximum bucket index (27 for 128MB buffers)
[[maybe_unused]] constexpr u8 CMaxBuckets     = CMaxBucketIndex + 1u;    //!< Total bucket indices
constexpr u64                 CMaxBufferSize  = 1u << CMaxBucketIndex;   //!< Maximum buffer size (128MB)
constexpr u32                 CHeaderSize     = sizeof(buffer_header_t); //!< Size of buffer header (8 bytes)

static_assert(sizeof(free_node_t) <= (1u << CMinBucketIndex), "Free node must fit in minimum buffer size");

//! Global metadata pointer
metadata_t* g_metadata = nullptr;

#if !SKL_BUILD_SHIPPING
//! [Debug] Track all currently allocated buffers for validation
std::unordered_set<void*> g_allocated_buffers[CMaxBuckets];
#endif
} // namespace

namespace {
//! Slow path: Allocate a new buffer page and link all buffers into the freelist
template <u32 BucketIndex>
[[gnu::noinline]] void populate_bucket_with_buffers() noexcept {
    static_assert(BucketIndex >= CMinBucketIndex && BucketIndex <= CMaxBucketIndex, "Invalid bucket index");

    constexpr u32 CBufferSize      = 1u << BucketIndex;
    constexpr u32 CBuffersToCreate = skl::huge_pages::CHugePageSize / CBufferSize;

    static_assert(CBufferSize >= sizeof(free_node_t), "Buffer must fit free node");

    if constexpr (CBufferSize <= skl::huge_pages::CHugePageSize) {
        // Allocate 1 huge page for buffers
        void* buffer_page = skl::huge_pages::skl_huge_page_alloc(1);
        SKL_ASSERT_PERMANENT(nullptr != buffer_page);

        // Track allocation
        SKL_ASSERT_PERMANENT(!g_metadata->allpages.full() && "Huge page tracking limit reached (128GB)");
        g_metadata->allpages.upgrade().push_back({buffer_page, 1});

        // Link all buffers into the intrusive freelist (prepend to existing chain)
        free_node_t* head       = g_metadata->bucket_heads[BucketIndex];
        byte*        buffer_ptr = reinterpret_cast<byte*>(buffer_page);

        for (u32 i = 0; i < CBuffersToCreate; ++i) {
            auto* node  = reinterpret_cast<free_node_t*>(buffer_ptr);
            node->next  = head;
            head        = node;
            buffer_ptr += CBufferSize;
        }

        g_metadata->bucket_heads[BucketIndex] = head;
    } else {
        // Large buffer: allocate multiple contiguous huge pages (1 buffer per allocation)
        constexpr u64 CPageCount = skl::integral_ceil<u64>(CBufferSize, skl::huge_pages::CHugePageSize);

        void* buffer = skl::huge_pages::skl_huge_page_alloc(CPageCount);
        SKL_ASSERT_PERMANENT(nullptr != buffer);

        // Track allocation with actual page count
        SKL_ASSERT_PERMANENT(!g_metadata->allpages.full() && "Huge page tracking limit reached (128GB)");
        g_metadata->allpages.upgrade().push_back({buffer, CPageCount});

        // Add single buffer to freelist
        auto* node                            = reinterpret_cast<free_node_t*>(buffer);
        node->next                            = g_metadata->bucket_heads[BucketIndex];
        g_metadata->bucket_heads[BucketIndex] = node;
    }
}

//! Fast path: Allocate from bucket's intrusive freelist
template <u32 BucketIndex>
void* allocate_from_bucket() noexcept {
    free_node_t* head = g_metadata->bucket_heads[BucketIndex];

    // Slow path: bucket empty, need to allocate new buffer page
    if (head == nullptr) [[unlikely]] {
        populate_bucket_with_buffers<BucketIndex>();
        head = g_metadata->bucket_heads[BucketIndex];
    }

    // Pop head from freelist
    g_metadata->bucket_heads[BucketIndex] = head->next;
    return head;
}

//! Fast path: Free buffer back to bucket's intrusive freelist
template <u32 BucketIndex>
void free_to_bucket(void* f_buffer) noexcept {
    // Push to head of freelist
    auto* node                            = reinterpret_cast<free_node_t*>(f_buffer);
    node->next                            = g_metadata->bucket_heads[BucketIndex];
    g_metadata->bucket_heads[BucketIndex] = node;
}

#if !SKL_BUILD_SHIPPING
//! [Debug] Validate that a buffer pointer is valid and came from this pool
[[gnu::noinline]] void validate_buffer_for_free(void* f_buffer, u32 f_bucket_index) noexcept {
    SKL_ASSERT_PERMANENT(nullptr != f_buffer);

    // Validate buffer was actually allocated from this specific bucket
    auto&      bucket_set   = g_allocated_buffers[f_bucket_index];
    const bool is_allocated = bucket_set.contains(f_buffer);
    SKL_ASSERT_PERMANENT(is_allocated && "Buffer not found in allocated set - double free or wrong bucket");

    // Remove from allocated set
    bucket_set.erase(f_buffer);
}
#endif
} // namespace

namespace skl {
[[nodiscard]] skl_result<u32> HugePageBufferPool::buffer_get_pool_index(u32 f_size) noexcept {
    SKL_ASSERT(f_size <= CMaxBufferSize);

    // Edge case: size 0 or 1 maps to minimum bucket
    if (f_size <= 1u) [[unlikely]] {
        return CMinBucketIndex;
    }

    // O(1) bucket calculation using CLZ
    // bucket = ceil(log2(f_size)) = 32 - clz(f_size - 1)
    const u32 bucket = 32u - u32(__builtin_clz(f_size - 1u));

    // Clamp to minimum bucket (sizes 1-32 all map to bucket 5)
    return (bucket < CMinBucketIndex) ? CMinBucketIndex : bucket;
}

skl_status HugePageBufferPool::construct_pool() noexcept {
    // Check if already initialized
    if (nullptr != g_metadata) {
        return SKL_ERR_STATE;
    }

    // Allocate metadata structure (1 huge page)
    g_metadata = reinterpret_cast<metadata_t*>(huge_pages::skl_huge_page_alloc(1));
    if (nullptr == g_metadata) {
        return SKL_ERR_ALLOC;
    }

    // Initialize metadata
    new (g_metadata) metadata_t();

    return SKL_SUCCESS;
}

void HugePageBufferPool::destroy_pool() noexcept {
    if (nullptr == g_metadata) {
        return;
    }

    // Free all tracked buffer pages
    for (const auto& page : g_metadata->allpages) {
        if (page.ptr != nullptr) {
            huge_pages::skl_huge_page_free(page.ptr, page.page_count);
        }
    }

    // Destroy and free metadata
    g_metadata->~metadata_t();
    huge_pages::skl_huge_page_free(g_metadata, 1);
    g_metadata = nullptr;
}

HugePageBufferPool::buffer_t HugePageBufferPool::buffer_alloc(u32 f_size) noexcept {
    // Max requestable size accounts for header overhead
    constexpr u32 CMaxRequestSize = CMaxBufferSize - CHeaderSize;

    SKL_ASSERT_PERMANENT((nullptr != g_metadata) && (f_size <= CMaxRequestSize));

    // Safe to add now - overflow is impossible after the above check
    const u32 total_size = f_size + CHeaderSize;

    const u32 bucket_index = buffer_get_pool_index(total_size).value();
    const u32 actual_size  = buffer_get_size_for_bucket(bucket_index);

    void* ptr = nullptr;

    // Dispatch to bucket
    switch (bucket_index) {
        case 5:
            ptr = allocate_from_bucket<5>();
            break;
        case 6:
            ptr = allocate_from_bucket<6>();
            break;
        case 7:
            ptr = allocate_from_bucket<7>();
            break;
        case 8:
            ptr = allocate_from_bucket<8>();
            break;
        case 9:
            ptr = allocate_from_bucket<9>();
            break;
        case 10:
            ptr = allocate_from_bucket<10>();
            break;
        case 11:
            ptr = allocate_from_bucket<11>();
            break;
        case 12:
            ptr = allocate_from_bucket<12>();
            break;
        case 13:
            ptr = allocate_from_bucket<13>();
            break;
        case 14:
            ptr = allocate_from_bucket<14>();
            break;
        case 15:
            ptr = allocate_from_bucket<15>();
            break;
        case 16:
            ptr = allocate_from_bucket<16>();
            break;
        case 17:
            ptr = allocate_from_bucket<17>();
            break;
        case 18:
            ptr = allocate_from_bucket<18>();
            break;
        case 19:
            ptr = allocate_from_bucket<19>();
            break;
        case 20:
            ptr = allocate_from_bucket<20>();
            break;
        case 21:
            ptr = allocate_from_bucket<21>();
            break;
        case 22:
            ptr = allocate_from_bucket<22>();
            break;
        case 23:
            ptr = allocate_from_bucket<23>();
            break;
        case 24:
            ptr = allocate_from_bucket<24>();
            break;
        case 25:
            ptr = allocate_from_bucket<25>();
            break;
        case 26:
            ptr = allocate_from_bucket<26>();
            break;
        case 27:
            ptr = allocate_from_bucket<27>();
            break;
        default:
            SKL_ASSERT_PERMANENT(false && "Invalid bucket index");
    }

    if (nullptr == ptr) [[unlikely]] {
        return {};
    }

    // Write header at start of buffer
    auto* header           = reinterpret_cast<buffer_header_t*>(ptr);
    header->allocated_size = actual_size;
#if !SKL_BUILD_SHIPPING
    header->magic = CBufferMagic;
#endif

    // Rebase pointer past header for user
    byte*     user_ptr    = reinterpret_cast<byte*>(ptr) + CHeaderSize;
    const u32 usable_size = actual_size - CHeaderSize;

#if !SKL_BUILD_SHIPPING
    g_allocated_buffers[bucket_index].insert(user_ptr);
#endif

    return buffer_t{usable_size, user_ptr};
}

void HugePageBufferPool::buffer_free(buffer_t f_alloc) noexcept {
    HugePageBufferPool::buffer_free_ptr(f_alloc.buffer);
}

void HugePageBufferPool::buffer_free_ptr(void* f_ptr) noexcept {
    SKL_ASSERT_PERMANENT((nullptr != g_metadata)
                         && "HugePageBufferPool already destroyed: free all hugepage allocations before skl_core_deinit()");
    SKL_ASSERT_PERMANENT(nullptr != f_ptr);

    // Get header from user pointer (rebase back)
    auto* header = reinterpret_cast<buffer_header_t*>(static_cast<byte*>(f_ptr) - CHeaderSize);

#if !SKL_BUILD_SHIPPING
    // Validate magic number to detect corruption/double-free
    SKL_ASSERT_PERMANENT((header->magic == CBufferMagic) && "Invalid buffer header - corruption or double-free");
    header->magic = 0; // Clear magic to detect double-free
#endif

    // Get bucket from stored size (not user-provided!)
    const u32 actual_size  = header->allocated_size;
    const u32 bucket_index = buffer_get_pool_index(actual_size).value();

#if !SKL_BUILD_SHIPPING
    validate_buffer_for_free(f_ptr, bucket_index);
#endif

    // Free the raw pointer (header start, not user pointer)
    void* raw_ptr = header;

    // Dispatch to bucket
    switch (bucket_index) {
        case 5:
            free_to_bucket<5>(raw_ptr);
            break;
        case 6:
            free_to_bucket<6>(raw_ptr);
            break;
        case 7:
            free_to_bucket<7>(raw_ptr);
            break;
        case 8:
            free_to_bucket<8>(raw_ptr);
            break;
        case 9:
            free_to_bucket<9>(raw_ptr);
            break;
        case 10:
            free_to_bucket<10>(raw_ptr);
            break;
        case 11:
            free_to_bucket<11>(raw_ptr);
            break;
        case 12:
            free_to_bucket<12>(raw_ptr);
            break;
        case 13:
            free_to_bucket<13>(raw_ptr);
            break;
        case 14:
            free_to_bucket<14>(raw_ptr);
            break;
        case 15:
            free_to_bucket<15>(raw_ptr);
            break;
        case 16:
            free_to_bucket<16>(raw_ptr);
            break;
        case 17:
            free_to_bucket<17>(raw_ptr);
            break;
        case 18:
            free_to_bucket<18>(raw_ptr);
            break;
        case 19:
            free_to_bucket<19>(raw_ptr);
            break;
        case 20:
            free_to_bucket<20>(raw_ptr);
            break;
        case 21:
            free_to_bucket<21>(raw_ptr);
            break;
        case 22:
            free_to_bucket<22>(raw_ptr);
            break;
        case 23:
            free_to_bucket<23>(raw_ptr);
            break;
        case 24:
            free_to_bucket<24>(raw_ptr);
            break;
        case 25:
            free_to_bucket<25>(raw_ptr);
            break;
        case 26:
            free_to_bucket<26>(raw_ptr);
            break;
        case 27:
            free_to_bucket<27>(raw_ptr);
            break;
        default:
            SKL_ASSERT_PERMANENT(false && "Invalid bucket index");
    }
}
} // namespace skl
