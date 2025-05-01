//!
//! \file skl_siphash_8
//!
//! \brief SipHash based hashing
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_hash"
#include "skl_int"

/* default: SipHash-2-4 */
#define SKL_HALF_SIPHASH_C_ROUNDS 2
#define SKL_HALF_SIPHASH_D_ROUNDS 4

#define SKL_HALF_SIPHASH_ROTL(x, b) (u32)(((x) << (b)) | ((x) >> (32 - (b))))

#define SKL_HALF_SIPHASH_U32TO8_LE(p, v) \
    (p)[0] = (byte)((v));                \
    (p)[1] = (byte)((v) >> 8);           \
    (p)[2] = (byte)((v) >> 16);          \
    (p)[3] = (byte)((v) >> 24);

#define SKL_HALF_SIPHASH_U32TO8_XOR_LE(p, v) \
    (p)[0] ^= (byte)((v));                   \
    (p)[1] ^= (byte)((v) >> 8);              \
    (p)[2] ^= (byte)((v) >> 16);             \
    (p)[3] ^= (byte)((v) >> 24);

#define SKL_HALF_SIPHASH_U8TO32_LE(p) \
    (((u32)((p)[0])) | ((u32)((p)[1]) << 8) | ((u32)((p)[2]) << 16) | ((u32)((p)[3]) << 24))

#define SKL_HALF_SIPHASH_SIPROUND            \
    do {                                     \
        v0 += v1;                            \
        v1  = SKL_HALF_SIPHASH_ROTL(v1, 5);  \
        v1 ^= v0;                            \
        v0  = SKL_HALF_SIPHASH_ROTL(v0, 16); \
        v2 += v3;                            \
        v3  = SKL_HALF_SIPHASH_ROTL(v3, 8);  \
        v3 ^= v2;                            \
        v0 += v3;                            \
        v3  = SKL_HALF_SIPHASH_ROTL(v3, 7);  \
        v3 ^= v0;                            \
        v2 += v1;                            \
        v1  = SKL_HALF_SIPHASH_ROTL(v1, 13); \
        v1 ^= v2;                            \
        v2  = SKL_HALF_SIPHASH_ROTL(v2, 16); \
    } while (0)

namespace skl {
void skl_siphash_8(const void* f_in, const void* f_key, void* f_out) noexcept {
    const byte* ni = (const byte*)f_in;
    const byte* kk = (const byte*)f_key;

    u32         v0   = 0;
    u32         v1   = 0;
    u32         v2   = u32(0x6c796765);
    u32         v3   = u32(0x74656462);
    u32         k0   = SKL_HALF_SIPHASH_U8TO32_LE(kk);
    u32         k1   = SKL_HALF_SIPHASH_U8TO32_LE(kk + 4);
    const byte* end  = ni + 8;
    v3              ^= k1;
    v2              ^= k0;
    v1              ^= k1;
    v0              ^= k0;

    constexpr u32 b = ((u32)8) << 24;

    v1 ^= u32(0xee);

    for (; ni != end; ni += 4) {
        u32 m  = SKL_HALF_SIPHASH_U8TO32_LE(ni);
        v3    ^= m;

        for (i32 i = 0; i < SKL_HALF_SIPHASH_C_ROUNDS; ++i) {
            SKL_HALF_SIPHASH_SIPROUND;
        }

        v0 ^= m;
    }

    v3 ^= b;

    for (i32 i = 0; i < SKL_HALF_SIPHASH_C_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    v0 ^= b;
    v2 ^= u32(0xee);

    for (i32 i = 0; i < SKL_HALF_SIPHASH_D_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    SKL_HALF_SIPHASH_U32TO8_LE(reinterpret_cast<byte*>(f_out), v1 ^ v3);

    v1 ^= u32(0xdd);

    for (i32 i = 0; i < SKL_HALF_SIPHASH_D_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    SKL_HALF_SIPHASH_U32TO8_LE(reinterpret_cast<byte*>(f_out) + 4, v1 ^ v3);
}
void skl_siphash_8_to_4(const void* f_in, const void* f_key, void* f_out_4) noexcept {
    const byte* ni = (const byte*)f_in;
    const byte* kk = (const byte*)f_key;

    u32         v0   = 0;
    u32         v1   = 0;
    u32         v2   = u32(0x6c796765);
    u32         v3   = u32(0x74656462);
    u32         k0   = SKL_HALF_SIPHASH_U8TO32_LE(kk);
    u32         k1   = SKL_HALF_SIPHASH_U8TO32_LE(kk + 4);
    const byte* end  = ni + 8;
    v3              ^= k1;
    v2              ^= k0;
    v1              ^= k1;
    v0              ^= k0;

    constexpr u32 b = ((u32)8) << 24;

    v1 ^= u32(0xee);

    for (; ni != end; ni += 4) {
        u32 m  = SKL_HALF_SIPHASH_U8TO32_LE(ni);
        v3    ^= m;

        for (i32 i = 0; i < SKL_HALF_SIPHASH_C_ROUNDS; ++i) {
            SKL_HALF_SIPHASH_SIPROUND;
        }

        v0 ^= m;
    }

    v3 ^= b;

    for (i32 i = 0; i < SKL_HALF_SIPHASH_C_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    v0 ^= b;
    v2 ^= u32(0xee);

    for (i32 i = 0; i < SKL_HALF_SIPHASH_D_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    SKL_HALF_SIPHASH_U32TO8_LE(reinterpret_cast<byte*>(f_out_4), v1 ^ v3);

    v1 ^= u32(0xdd);

    for (i32 i = 0; i < SKL_HALF_SIPHASH_D_ROUNDS; ++i) {
        SKL_HALF_SIPHASH_SIPROUND;
    }

    SKL_HALF_SIPHASH_U32TO8_XOR_LE(reinterpret_cast<byte*>(f_out_4), v1 ^ v3);
}
} // namespace skl

#undef SKL_HALF_SIPHASH_C_ROUNDS
#undef SKL_HALF_SIPHASH_D_ROUNDS
#undef SKL_HALF_SIPHASH_ROTL
#undef SKL_HALF_SIPHASH_U32TO8_LE
#undef SKL_HALF_SIPHASH_U8TO32_LE
#undef SKL_HALF_SIPHASH_U32TO8_XOR_LE
#undef SKL_HALF_SIPHASH_SIPROUND
