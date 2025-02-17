//!
//! \file skl_siphash_16
//!
//! \brief SipHash based hashing
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_hash"
#include "skl_int"

/* default: SipHash-2-4 */
#define SKL_SIPHASH_C_ROUNDS   2
#define SKL_SIPHASH_D_ROUNDS   4
#define SKL_SIPHASH_ROTL(x, b) (u64)(((x) << (b)) | ((x) >> (64 - (b))))

#define SKL_SIPHASH_U32TO8_LE(p, v) \
    (p)[0] = (byte)((v));           \
    (p)[1] = (byte)((v) >> 8);      \
    (p)[2] = (byte)((v) >> 16);     \
    (p)[3] = (byte)((v) >> 24);

#define SKL_SIPHASH_U64TO8_LE(p, v)         \
    SKL_SIPHASH_U32TO8_LE((p), (u32)((v))); \
    SKL_SIPHASH_U32TO8_LE((p) + 4, (u32)((v) >> 32));

#define SKL_SIPHASH_U8TO64_LE(p) \
    (((u64)((p)[0])) | ((u64)((p)[1]) << 8) | ((u64)((p)[2]) << 16) | ((u64)((p)[3]) << 24) | ((u64)((p)[4]) << 32) | ((u64)((p)[5]) << 40) | ((u64)((p)[6]) << 48) | ((u64)((p)[7]) << 56))

#define SKL_SIPHASH_SIPROUND            \
    do {                                \
        v0 += v1;                       \
        v1  = SKL_SIPHASH_ROTL(v1, 13); \
        v1 ^= v0;                       \
        v0  = SKL_SIPHASH_ROTL(v0, 32); \
        v2 += v3;                       \
        v3  = SKL_SIPHASH_ROTL(v3, 16); \
        v3 ^= v2;                       \
        v0 += v3;                       \
        v3  = SKL_SIPHASH_ROTL(v3, 21); \
        v3 ^= v0;                       \
        v2 += v1;                       \
        v1  = SKL_SIPHASH_ROTL(v1, 17); \
        v1 ^= v2;                       \
        v2  = SKL_SIPHASH_ROTL(v2, 32); \
    } while (0)

namespace skl {
void skl_siphash_16(const void* f_in, const void* f_key, void* f_out) noexcept {
    const byte* ni = (const byte*)f_in;
    const byte* kk = (const byte*)f_key;

    u64         v0 = u64(0x736f6d6570736575);
    u64         v1 = u64(0x646f72616e646f6d);
    u64         v2 = u64(0x6c7967656e657261);
    u64         v3 = u64(0x7465646279746573);
    u64         k0 = SKL_SIPHASH_U8TO64_LE(kk);
    u64         k1 = SKL_SIPHASH_U8TO64_LE(kk + 8);
    const byte* end  = ni + 16;
    v3              ^= k1;
    v2              ^= k0;
    v1              ^= k1;
    v0              ^= k0;

    constexpr u64 b = ((u64)16) << 56;

    v1 ^= u64(0xee);

    for (; ni != end; ni += 8) {
        u64 m = SKL_SIPHASH_U8TO64_LE(ni);

        v3 ^= m;

        for (i32 i = 0; i < SKL_SIPHASH_C_ROUNDS; ++i) {
            SKL_SIPHASH_SIPROUND;
        }

        v0 ^= m;
    }

    v3 ^= b;

    for (i32 i = 0; i < SKL_SIPHASH_C_ROUNDS; ++i) {
        SKL_SIPHASH_SIPROUND;
    }

    v0 ^= b;
    v2 ^= u64(0xee);

    for (i32 i = 0; i < SKL_SIPHASH_D_ROUNDS; ++i) {
        SKL_SIPHASH_SIPROUND;
    }

    SKL_SIPHASH_U64TO8_LE(reinterpret_cast<byte*>(f_out), (v0 ^ v1 ^ v2 ^ v3));

    v1 ^= u64(0xdd);

    for (i32 i = 0; i < SKL_SIPHASH_D_ROUNDS; ++i) {
        SKL_SIPHASH_SIPROUND;
    }

    SKL_SIPHASH_U64TO8_LE(reinterpret_cast<byte*>(f_out) + 8, (v0 ^ v1 ^ v2 ^ v3));
}
} // namespace skl

#undef SKL_SIPHASH_C_ROUNDS
#undef SKL_SIPHASH_D_ROUNDS
#undef SKL_SIPHASH_ROTL
#undef SKL_SIPHASH_U64TO8_LE
#undef SKL_SIPHASH_U8TO64_LE
#undef SKL_SIPHASH_SIPROUND
