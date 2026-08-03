//!
//! \file skl_secure_rand
//!
//! \brief AES-128-CTR DRBG with fast key erasure, per thread, no syscalls after seeding
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <immintrin.h>
#include <wmmintrin.h>
#include <sys/random.h>
#include <cerrno>
#include <cstring>
#include <strings.h>

#include "skl_secure_rand"
#include "skl_assert"
#include "skl_status"

namespace {
//! AES block size, also the DRBG key size
constexpr u64 CAesBlockSize = 16ULL;

//! Bytes produced per aes_ctr_generate() step (8 blocks kept in flight)
constexpr u64 CAesPipelineStride = CAesBlockSize * 8ULL;

static_assert(0ULL == (skl::CSecureRandBufferSize % CAesPipelineStride),
              "CSecureRandBufferSize must be a multiple of the 8 block pipeline stride");

//! DRBG state. Plain thread_local PODs - no allocation, no destructor, nothing to leak and
//! no teardown contract to get wrong. The buffer lives in .tbss so a thread that never asks
//! for secure bytes never faults the pages in.
//!
//! \warning NOT fork safe by design - see skl_secure_rand for the contract
alignas(16) thread_local __m128i g_round_keys[11];
alignas(64) thread_local byte    g_buffer[skl::CSecureRandBufferSize];
thread_local u64                 g_counter = 0ULL;
thread_local u64                 g_cursor  = skl::CSecureRandBufferSize; //!< starts exhausted
thread_local bool                g_seeded  = false;

//! One AES-128 key schedule round
#define SKL_AES_EXPAND_STEP(f_key, f_rcon)                        \
    do {                                                          \
        __m128i gen = _mm_aeskeygenassist_si128((f_key), (f_rcon)); \
        gen         = _mm_shuffle_epi32(gen, 0xFF);               \
        __m128i shf = _mm_slli_si128((f_key), 4);                 \
        (f_key)     = _mm_xor_si128((f_key), shf);                \
        shf         = _mm_slli_si128(shf, 4);                     \
        (f_key)     = _mm_xor_si128((f_key), shf);                \
        shf         = _mm_slli_si128(shf, 4);                     \
        (f_key)     = _mm_xor_si128((f_key), shf);                \
        (f_key)     = _mm_xor_si128((f_key), gen);                \
    } while (false)

//! Install a new AES-128 key schedule from 16 key bytes
[[gnu::target("aes")]] void skl_aes_rekey(const byte* f_key) noexcept {
    __m128i key       = _mm_loadu_si128(reinterpret_cast<const __m128i*>(f_key));
    g_round_keys[0]   = key;
    SKL_AES_EXPAND_STEP(key, 0x01); g_round_keys[1]  = key;
    SKL_AES_EXPAND_STEP(key, 0x02); g_round_keys[2]  = key;
    SKL_AES_EXPAND_STEP(key, 0x04); g_round_keys[3]  = key;
    SKL_AES_EXPAND_STEP(key, 0x08); g_round_keys[4]  = key;
    SKL_AES_EXPAND_STEP(key, 0x10); g_round_keys[5]  = key;
    SKL_AES_EXPAND_STEP(key, 0x20); g_round_keys[6]  = key;
    SKL_AES_EXPAND_STEP(key, 0x40); g_round_keys[7]  = key;
    SKL_AES_EXPAND_STEP(key, 0x80); g_round_keys[8]  = key;
    SKL_AES_EXPAND_STEP(key, 0x1B); g_round_keys[9]  = key;
    SKL_AES_EXPAND_STEP(key, 0x36); g_round_keys[10] = key;
}

//! Encrypt the counter sequence into f_target under the current key schedule
//! \remark f_size must be a multiple of CAesPipelineStride
[[gnu::target("aes")]] void skl_aes_ctr_generate(byte* f_target, u64 f_size) noexcept {
    for (u64 offset = 0ULL; offset < f_size; offset += CAesPipelineStride) {
        __m128i blocks[8];

        //Eight independent chains keep the aesenc pipeline saturated (4 cycle latency,
        //1 per cycle throughput)
        for (u64 i = 0ULL; i < 8ULL; ++i) {
            blocks[i] = _mm_set_epi64x(0, static_cast<long long>(g_counter + i));
        }
        g_counter += 8ULL;

        for (auto& block : blocks) {
            block = _mm_xor_si128(block, g_round_keys[0]);
        }
        for (u64 round = 1ULL; round < 10ULL; ++round) {
            for (auto& block : blocks) {
                block = _mm_aesenc_si128(block, g_round_keys[round]);
            }
        }
        for (auto& block : blocks) {
            block = _mm_aesenclast_si128(block, g_round_keys[10]);
        }

        for (u64 i = 0ULL; i < 8ULL; ++i) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(f_target + offset + (i * CAesBlockSize)), blocks[i]);
        }
    }
}

//! Draw exactly f_size bytes from the kernel CSPRNG
//! \remark Seeding only - never on the steady state path
void skl_fill_from_kernel(byte* f_target, u64 f_size) noexcept {
    u64 filled = 0ULL;

    while (filled < f_size) {
        const auto result = ::getrandom(f_target + filled, f_size - filled, 0);
        if (result <= 0) [[unlikely]] {
            //EINTR is the only condition worth retrying. Anything else means the kernel
            //CSPRNG is unusable, and handing back predictable bytes as if they were secure
            //is strictly worse than dying here
            SKL_ASSERT_PERMANENT((result < 0) && (EINTR == errno));
            continue;
        }

        filled += static_cast<u64>(result);
    }
}

//! Refill the buffer and rotate the key
[[gnu::target("aes")]] void skl_drbg_refill() noexcept {
    skl_aes_ctr_generate(g_buffer, skl::CSecureRandBufferSize);

    //Fast key erasure: the leading block becomes the next key and is never issued, so the
    //state cannot be rolled backwards to reproduce anything already handed out
    skl_aes_rekey(g_buffer);
    ::explicit_bzero(g_buffer, CAesBlockSize);

    //Safe to restart the counter - it is namespaced by a key that has never been used before
    g_counter = 0ULL;
    g_cursor  = CAesBlockSize;
}

//! Seed this thread's DRBG from the kernel CSPRNG
void skl_drbg_seed() noexcept {
    //The DRBG has no software AES fallback on purpose - a fallback would either be slow or
    //not be a CSPRNG, and every supported target (x86-64 server silicon) has AES-NI
    SKL_ASSERT_PERMANENT(0 != __builtin_cpu_supports("aes"));

    byte key[CAesBlockSize];
    skl_fill_from_kernel(key, CAesBlockSize);
    skl_aes_rekey(key);
    ::explicit_bzero(key, CAesBlockSize);

    g_counter = 0ULL;
    g_cursor  = skl::CSecureRandBufferSize; //!< force a refill on the first request
    g_seeded  = true;
}
} // namespace

namespace skl {
void secure_random_bytes(void* f_target, u64 f_size) noexcept {
    SKL_ASSERT(nullptr != f_target);

    if (false == g_seeded) [[unlikely]] {
        //Only reachable on a thread that skipped skl_core_init_thread()
        skl_drbg_seed();
    }

    auto* target   = static_cast<byte*>(f_target);
    u64   produced = 0ULL;

    while (produced < f_size) {
        if (g_cursor >= CSecureRandBufferSize) [[unlikely]] {
            skl_drbg_refill();
        }

        const u64 available = CSecureRandBufferSize - g_cursor;
        const u64 remaining = f_size - produced;
        const u64 chunk     = (remaining < available) ? remaining : available;

        std::memcpy(target + produced, g_buffer + g_cursor, chunk);

        //Fast key erasure - issued bytes do not survive in the buffer, so a later disclosure
        //of this memory reveals nothing about ids already handed out
        std::memset(g_buffer + g_cursor, 0, chunk);

        g_cursor += chunk;
        produced += chunk;
    }
}

void g_secure_random_bytes(void* f_target, u64 f_size) noexcept {
    SKL_ASSERT(nullptr != f_target);
    skl_fill_from_kernel(static_cast<byte*>(f_target), f_size);
}
} // namespace skl

namespace skl {
//! \remark There is no matching deinit - the state is static thread storage, so there is
//!         nothing to free
skl_status skl_core_init_thread__csprng() noexcept {
    //Seed eagerly so that no thread ever issues a getrandom(2) once the server is running
    skl_drbg_seed();
    return SKL_SUCCESS;
}
} // namespace skl
