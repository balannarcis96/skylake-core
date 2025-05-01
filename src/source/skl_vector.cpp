//!
//! \file skl_vector
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstring>

#include <mimalloc.h>

#include <tune_skl_core_public.h>

#include "skl_int"

namespace skl {
void* skl_vector_alloc(u64 f_bytes_count, u64 f_alignment) noexcept {
    return mi_malloc_aligned(f_bytes_count, f_alignment);
}
void skl_vector_free(void* f_block) noexcept {
    mi_free(f_block);
}
#if !SKL_CORE_EXTERNAL_ALLOC
void* skl_core_alloc(u64 f_bytes_count, u64 f_alignment) noexcept {
    return mi_malloc_aligned(f_bytes_count, f_alignment);
}
void skl_core_free(void* f_block) noexcept {
    mi_free(f_block);
}
#endif
void skl_vector_memcpy(void* f_dest, const void* f_src, u64 f_bytes_count) noexcept {
    (void)memcpy(f_dest, f_src, f_bytes_count);
}
void skl_core_zero_memory(void * f_dest, u64 f_bytes_count) noexcept{
    (void)memset(f_dest, 0, f_bytes_count);
}
} // namespace skl
