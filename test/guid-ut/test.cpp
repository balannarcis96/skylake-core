#include <skl_guid>
#include <skl_sguid>
#include <skl_sguid64>
#include <skl_buffer_view>
#include <skl_string_view>

#include <gtest/gtest.h>

#include <cstring>

// ============================================================================
// GUID Tests
// ============================================================================

TEST(GUID, DefaultConstructionIsNull) {
    skl::GUID guid{};
    EXPECT_TRUE(guid.is_null());
}

TEST(GUID, NullConstantIsNull) {
    EXPECT_TRUE(skl::GUID_Zero.is_null());
}

TEST(GUID, MaxConstantIsNotNull) {
    EXPECT_FALSE(skl::GUID_Max.is_null());
}

TEST(GUID, ConstructFromU64Parts) {
    skl::GUID guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};
    EXPECT_FALSE(guid.is_null());

    const auto [lo, hi] = guid.raw();
    EXPECT_EQ(lo, 0x0102030405060708ull);
    EXPECT_EQ(hi, 0x090a0b0c0d0e0f10ull);
}

TEST(GUID, EqualityOperator) {
    skl::GUID guid1{0x123456789abcdef0ull, 0xfedcba9876543210ull};
    skl::GUID guid2{0x123456789abcdef0ull, 0xfedcba9876543210ull};
    skl::GUID guid3{};

    EXPECT_EQ(guid1, guid2);
    EXPECT_NE(guid1, guid3);
    EXPECT_EQ(guid3, skl::GUID_Zero);
}

TEST(GUID, ToStringBufferNullGUID) {
    skl::GUID guid{};
    char      buffer[64];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = guid.to_string(buffer_view);

    EXPECT_EQ(len, 32u);
    EXPECT_STREQ(buffer, "00000000000000000000000000000000");
    EXPECT_EQ(std::strlen(buffer), 32u);
}

TEST(GUID, ToStringBufferNonNullGUID) {
    // Create a GUID with known byte values
    skl::GUID guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};
    char      buffer[64];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = guid.to_string(buffer_view);

    EXPECT_EQ(len, 32u);
    EXPECT_STREQ(buffer, "0807060504030201100f0e0d0c0b0a09");
    EXPECT_EQ(std::strlen(buffer), 32u);
}

TEST(GUID, ToStringViewNullGUID) {
    skl::GUID guid{};

    const skl::skl_string_view str = guid.to_string();

    EXPECT_EQ(str.length(), 32u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "00000000000000000000000000000000");
}

TEST(GUID, ToStringViewNonNullGUID) {
    skl::GUID guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};

    const skl::skl_string_view str = guid.to_string();

    EXPECT_EQ(str.length(), 32u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "0807060504030201100f0e0d0c0b0a09");
}

TEST(GUID, ToStringFancyBufferNullGUID) {
    skl::GUID guid{};
    char      buffer[64];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = guid.to_string_fancy(buffer_view);

    EXPECT_EQ(len, 36u);
    EXPECT_STREQ(buffer, "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(std::strlen(buffer), 36u);
}

TEST(GUID, ToStringFancyBufferNonNullGUID) {
    skl::GUID guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};
    char      buffer[64];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = guid.to_string_fancy(buffer_view);

    EXPECT_EQ(len, 36u);
    EXPECT_STREQ(buffer, "08070605-0403-0201-100f-0e0d0c0b0a09");
    EXPECT_EQ(std::strlen(buffer), 36u);
}

TEST(GUID, ToStringFancyViewNullGUID) {
    skl::GUID guid{};

    const skl::skl_string_view str = guid.to_string_fancy();

    EXPECT_EQ(str.length(), 36u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "00000000-0000-0000-0000-000000000000");
}

TEST(GUID, ToStringFancyViewNonNullGUID) {
    skl::GUID guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};

    const skl::skl_string_view str = guid.to_string_fancy();

    EXPECT_EQ(str.length(), 36u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "08070605-0403-0201-100f-0e0d0c0b0a09");
}

TEST(GUID, MakeGUID) {
    skl::GUID guid1 = skl::make_guid();
    skl::GUID guid2 = skl::make_guid();

    EXPECT_FALSE(guid1.is_null());
    EXPECT_FALSE(guid2.is_null());
    // Very unlikely to be equal (collision probability is astronomically low)
    EXPECT_NE(guid1, guid2);
}

TEST(GUID, MakeGUIDFast) {
    skl::GUID guid1 = skl::make_guid_fast();
    skl::GUID guid2 = skl::make_guid_fast();

    EXPECT_FALSE(guid1.is_null());
    EXPECT_FALSE(guid2.is_null());
    EXPECT_NE(guid1, guid2);
}

TEST(GUID, GlobalMakeGUID) {
    skl::GUID guid1 = skl::g_make_guid();
    skl::GUID guid2 = skl::g_make_guid();

    EXPECT_FALSE(guid1.is_null());
    EXPECT_FALSE(guid2.is_null());
    EXPECT_NE(guid1, guid2);
}

TEST(GUID, GlobalMakeGUIDFast) {
    skl::GUID guid1 = skl::g_make_guid_fast();
    skl::GUID guid2 = skl::g_make_guid_fast();

    EXPECT_FALSE(guid1.is_null());
    EXPECT_FALSE(guid2.is_null());
    EXPECT_NE(guid1, guid2);
}

TEST(GUID, HashFunction) {
    skl::GUID guid{0x123456789abcdef0ull, 0xfedcba9876543210ull};

    const u64 hash = skl::guid_hash::operator()(guid);

    // Hash should be XOR of low and high parts
    EXPECT_EQ(hash, 0x123456789abcdef0ull ^ 0xfedcba9876543210ull);
}

TEST(GUID, CopyGUIDRaw) {
    skl::GUID_raw_t in{u8(0x01), u8(0x02), u8(0x03), u8(0x04), u8(0x05), u8(0x06), u8(0x07), u8(0x08),
                       u8(0x09), u8(0x0a), u8(0x0b), u8(0x0c), u8(0x0d), u8(0x0e), u8(0x0f), u8(0x10)};
    skl::GUID_raw_t out{};

    skl::copy_guid_raw(in, out);

    for (byte i = 0u; i < skl::CGUIDSize; ++i) {
        EXPECT_EQ(out[i], in[i]);
    }
}

TEST(GUID, RawBytesExtraction) {
    skl::GUID       guid{0x0102030405060708ull, 0x090a0b0c0d0e0f10ull};
    skl::GUID_raw_t raw_bytes{};

    guid.raw(raw_bytes);

    // Verify little-endian byte order
    EXPECT_EQ(raw_bytes[0], u8(0x08));
    EXPECT_EQ(raw_bytes[1], u8(0x07));
    EXPECT_EQ(raw_bytes[2], u8(0x06));
    EXPECT_EQ(raw_bytes[3], u8(0x05));
    EXPECT_EQ(raw_bytes[4], u8(0x04));
    EXPECT_EQ(raw_bytes[5], u8(0x03));
    EXPECT_EQ(raw_bytes[6], u8(0x02));
    EXPECT_EQ(raw_bytes[7], u8(0x01));
    EXPECT_EQ(raw_bytes[8], u8(0x10));
    EXPECT_EQ(raw_bytes[9], u8(0x0f));
    EXPECT_EQ(raw_bytes[10], u8(0x0e));
    EXPECT_EQ(raw_bytes[11], u8(0x0d));
    EXPECT_EQ(raw_bytes[12], u8(0x0c));
    EXPECT_EQ(raw_bytes[13], u8(0x0b));
    EXPECT_EQ(raw_bytes[14], u8(0x0a));
    EXPECT_EQ(raw_bytes[15], u8(0x09));
}

// ============================================================================
// SGUID Tests
// ============================================================================

TEST(SGUID, DefaultConstructionIsNull) {
    skl::SGUID sguid{};
    EXPECT_TRUE(sguid.is_null());
}

TEST(SGUID, NullConstantIsNull) {
    EXPECT_TRUE(skl::SGUID_Zero.is_null());
}

TEST(SGUID, MaxConstantIsNotNull) {
    EXPECT_FALSE(skl::SGUID_Max.is_null());
}

TEST(SGUID, ConstructFromU32) {
    skl::SGUID sguid{0x01020304u};
    EXPECT_FALSE(sguid.is_null());
    EXPECT_EQ(sguid.raw(), 0x01020304u);
}

TEST(SGUID, ConstructFromBytes) {
    skl::SGUID sguid{u8(0x01), u8(0x02), u8(0x03), u8(0x04)};
    EXPECT_FALSE(sguid.is_null());
    EXPECT_EQ(sguid[0], u8(0x01));
    EXPECT_EQ(sguid[1], u8(0x02));
    EXPECT_EQ(sguid[2], u8(0x03));
    EXPECT_EQ(sguid[3], u8(0x04));
}

TEST(SGUID, EqualityOperators) {
    skl::SGUID sguid1{0x12345678u};
    skl::SGUID sguid2{0x12345678u};
    skl::SGUID sguid3{};

    EXPECT_TRUE(sguid1 == sguid2);
    EXPECT_FALSE(sguid1 != sguid2);
    EXPECT_TRUE(sguid1 != sguid3);
    EXPECT_FALSE(sguid1 == sguid3);
    EXPECT_TRUE(sguid3 == skl::SGUID_Zero);
}

TEST(SGUID, ToStringBufferNullSGUID) {
    skl::SGUID sguid{};
    char       buffer[16];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = sguid.to_string(buffer_view);

    EXPECT_EQ(len, 8u);
    EXPECT_STREQ(buffer, "00000000");
    EXPECT_EQ(std::strlen(buffer), 8u);
}

TEST(SGUID, ToStringBufferNonNullSGUID) {
    skl::SGUID sguid{0x01020304u};
    char       buffer[16];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = sguid.to_string(buffer_view);

    EXPECT_EQ(len, 8u);
    EXPECT_STREQ(buffer, "04030201");
    EXPECT_EQ(std::strlen(buffer), 8u);
}

TEST(SGUID, ToStringViewNullSGUID) {
    skl::SGUID sguid{};

    const skl::skl_string_view str = sguid.to_string();

    EXPECT_EQ(str.length(), 8u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "00000000");
}

TEST(SGUID, ToStringViewNonNullSGUID) {
    skl::SGUID sguid{0x01020304u};

    const skl::skl_string_view str = sguid.to_string();

    EXPECT_EQ(str.length(), 8u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "04030201");
}

TEST(SGUID, MakeSGUID) {
    skl::SGUID sguid1 = skl::make_sguid();
    skl::SGUID sguid2 = skl::make_sguid();

    EXPECT_FALSE(sguid1.is_null());
    EXPECT_FALSE(sguid2.is_null());
    // Very likely to be different (though 4-byte collision is more likely than GUID)
    EXPECT_NE(sguid1, sguid2);
}

TEST(SGUID, MakeSGUIDFast) {
    skl::SGUID sguid1 = skl::make_sguid_fast();
    skl::SGUID sguid2 = skl::make_sguid_fast();

    EXPECT_FALSE(sguid1.is_null());
    EXPECT_FALSE(sguid2.is_null());
    EXPECT_NE(sguid1, sguid2);
}

TEST(SGUID, GlobalMakeSGUID) {
    skl::SGUID sguid1 = skl::g_make_sguid();
    skl::SGUID sguid2 = skl::g_make_sguid();

    EXPECT_FALSE(sguid1.is_null());
    EXPECT_FALSE(sguid2.is_null());
    EXPECT_NE(sguid1, sguid2);
}

TEST(SGUID, GlobalMakeSGUIDFast) {
    skl::SGUID sguid1 = skl::g_make_sguid_fast();
    skl::SGUID sguid2 = skl::g_make_sguid_fast();

    EXPECT_FALSE(sguid1.is_null());
    EXPECT_FALSE(sguid2.is_null());
    EXPECT_NE(sguid1, sguid2);
}

TEST(SGUID, HashFunction) {
    skl::SGUID sguid{0x12345678u};

    const u64 hash = skl::sguid_hash::operator()(sguid);

    EXPECT_EQ(hash, static_cast<u64>(0x12345678u));
}

TEST(SGUID, CopySGUIDRaw) {
    skl::SGUID_raw_t in{u8(0x01), u8(0x02), u8(0x03), u8(0x04)};
    skl::SGUID_raw_t out{};

    skl::copy_sguid_raw(in, out);

    for (byte i = 0u; i < skl::CSGUIDSize; ++i) {
        EXPECT_EQ(out[i], in[i]);
    }
}

// ============================================================================
// SGUID64 Tests
// ============================================================================

TEST(SGUID64, DefaultConstructionIsNull) {
    skl::SGUID64 sguid64{};
    EXPECT_TRUE(sguid64.is_null());
}

TEST(SGUID64, NullConstantIsNull) {
    EXPECT_TRUE(skl::SGUID64_Zero.is_null());
}

TEST(SGUID64, MaxConstantIsNotNull) {
    EXPECT_FALSE(skl::SGUID64_Max.is_null());
}

TEST(SGUID64, ConstructFromU64) {
    skl::SGUID64 sguid64{0x0102030405060708ull};
    EXPECT_FALSE(sguid64.is_null());
    EXPECT_EQ(sguid64.raw(), 0x0102030405060708ull);
}

TEST(SGUID64, ConstructFromBytes) {
    skl::SGUID64 sguid64{u8(0x01), u8(0x02), u8(0x03), u8(0x04), u8(0x05), u8(0x06), u8(0x07), u8(0x08)};
    EXPECT_FALSE(sguid64.is_null());
    EXPECT_EQ(sguid64[0], u8(0x01));
    EXPECT_EQ(sguid64[1], u8(0x02));
    EXPECT_EQ(sguid64[2], u8(0x03));
    EXPECT_EQ(sguid64[3], u8(0x04));
    EXPECT_EQ(sguid64[4], u8(0x05));
    EXPECT_EQ(sguid64[5], u8(0x06));
    EXPECT_EQ(sguid64[6], u8(0x07));
    EXPECT_EQ(sguid64[7], u8(0x08));
}

TEST(SGUID64, EqualityOperators) {
    skl::SGUID64 sguid64_1{0x123456789abcdef0ull};
    skl::SGUID64 sguid64_2{0x123456789abcdef0ull};
    skl::SGUID64 sguid64_3{};

    EXPECT_TRUE(sguid64_1 == sguid64_2);
    EXPECT_FALSE(sguid64_1 != sguid64_2);
    EXPECT_TRUE(sguid64_1 != sguid64_3);
    EXPECT_FALSE(sguid64_1 == sguid64_3);
    EXPECT_TRUE(sguid64_3 == skl::SGUID64_Zero);
}

TEST(SGUID64, ToStringBufferNullSGUID64) {
    skl::SGUID64 sguid64{};
    char         buffer[32];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = sguid64.to_string(buffer_view);

    EXPECT_EQ(len, 16u);
    EXPECT_STREQ(buffer, "0000000000000000");
    EXPECT_EQ(std::strlen(buffer), 16u);
}

TEST(SGUID64, ToStringBufferNonNullSGUID64) {
    skl::SGUID64 sguid64{0x0102030405060708ull};
    char         buffer[32];

    skl::skl_buffer_view buffer_view{buffer};
    const u64            len = sguid64.to_string(buffer_view);

    EXPECT_EQ(len, 16u);
    EXPECT_STREQ(buffer, "0807060504030201");
    EXPECT_EQ(std::strlen(buffer), 16u);
}

TEST(SGUID64, ToStringViewNullSGUID64) {
    skl::SGUID64 sguid64{};

    const skl::skl_string_view str = sguid64.to_string();

    EXPECT_EQ(str.length(), 16u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "0000000000000000");
}

TEST(SGUID64, ToStringViewNonNullSGUID64) {
    skl::SGUID64 sguid64{0x0102030405060708ull};

    const skl::skl_string_view str = sguid64.to_string();

    EXPECT_EQ(str.length(), 16u);
    EXPECT_EQ(std::string_view(str.data(), str.length()), "0807060504030201");
}

TEST(SGUID64, MakeSGUID64) {
    skl::SGUID64 sguid64_1 = skl::make_sguid64();
    skl::SGUID64 sguid64_2 = skl::make_sguid64();

    EXPECT_FALSE(sguid64_1.is_null());
    EXPECT_FALSE(sguid64_2.is_null());
    EXPECT_NE(sguid64_1, sguid64_2);
}

TEST(SGUID64, MakeSGUID64Fast) {
    skl::SGUID64 sguid64_1 = skl::make_sguid64_fast();
    skl::SGUID64 sguid64_2 = skl::make_sguid64_fast();

    EXPECT_FALSE(sguid64_1.is_null());
    EXPECT_FALSE(sguid64_2.is_null());
    EXPECT_NE(sguid64_1, sguid64_2);
}

TEST(SGUID64, GlobalMakeSGUID64) {
    skl::SGUID64 sguid64_1 = skl::g_make_sguid64();
    skl::SGUID64 sguid64_2 = skl::g_make_sguid64();

    EXPECT_FALSE(sguid64_1.is_null());
    EXPECT_FALSE(sguid64_2.is_null());
    EXPECT_NE(sguid64_1, sguid64_2);
}

TEST(SGUID64, GlobalMakeSGUID64Fast) {
    skl::SGUID64 sguid64_1 = skl::g_make_sguid64_fast();
    skl::SGUID64 sguid64_2 = skl::g_make_sguid64_fast();

    EXPECT_FALSE(sguid64_1.is_null());
    EXPECT_FALSE(sguid64_2.is_null());
    EXPECT_NE(sguid64_1, sguid64_2);
}

TEST(SGUID64, HashFunction) {
    skl::SGUID64 sguid64{0x123456789abcdef0ull};

    const u64 hash = skl::sguid64_hash::operator()(sguid64);

    EXPECT_EQ(hash, 0x123456789abcdef0ull);
}

TEST(SGUID64, CopySGUID64Raw) {
    skl::SGUID64_raw_t in{u8(0x01), u8(0x02), u8(0x03), u8(0x04), u8(0x05), u8(0x06), u8(0x07), u8(0x08)};
    skl::SGUID64_raw_t out{};

    skl::copy_sguid64_raw(in, out);

    for (byte i = 0u; i < skl::CSGUID64Size; ++i) {
        EXPECT_EQ(out[i], in[i]);
    }
}
