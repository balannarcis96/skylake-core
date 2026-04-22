#include <skl_base64>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <thread>

using skl::CBase64Error;
using skl::EBase64Alphabet;

// ============================================================================
// Size helpers (compile-time)
// ============================================================================

TEST(SklBase64, encoded_size_padded) {
    static_assert(skl::base64_encoded_size(0U) == 0U);
    static_assert(skl::base64_encoded_size(1U) == 4U);
    static_assert(skl::base64_encoded_size(2U) == 4U);
    static_assert(skl::base64_encoded_size(3U) == 4U);
    static_assert(skl::base64_encoded_size(4U) == 8U);
    static_assert(skl::base64_encoded_size(5U) == 8U);
    static_assert(skl::base64_encoded_size(6U) == 8U);
    static_assert(skl::base64_encoded_size(7U) == 12U);

    EXPECT_EQ(0u, skl::base64_encoded_size(0U));
    EXPECT_EQ(4u, skl::base64_encoded_size(3U));
    EXPECT_EQ(8u, skl::base64_encoded_size(4U));
}

TEST(SklBase64, encoded_size_unpadded) {
    static_assert(skl::base64_encoded_size(0U, false) == 0U);
    static_assert(skl::base64_encoded_size(1U, false) == 2U);
    static_assert(skl::base64_encoded_size(2U, false) == 3U);
    static_assert(skl::base64_encoded_size(3U, false) == 4U);
    static_assert(skl::base64_encoded_size(4U, false) == 6U);
    static_assert(skl::base64_encoded_size(5U, false) == 7U);
    static_assert(skl::base64_encoded_size(6U, false) == 8U);
    static_assert(skl::base64_encoded_size(7U, false) == 10U);
}

TEST(SklBase64, decoded_max_size) {
    static_assert(skl::base64_decoded_max_size(0U) == 0U);
    static_assert(skl::base64_decoded_max_size(2U) == 1U);
    static_assert(skl::base64_decoded_max_size(3U) == 2U);
    static_assert(skl::base64_decoded_max_size(4U) == 3U);
    static_assert(skl::base64_decoded_max_size(6U) == 4U);
    static_assert(skl::base64_decoded_max_size(7U) == 5U);
    static_assert(skl::base64_decoded_max_size(8U) == 6U);
}

// ============================================================================
// Runtime encode
// ============================================================================

TEST(SklBase64, encode_empty) {
    char out[4] = {'X', 'X', 'X', 'X'};
    const u64 n = skl::base64_encode<>(static_cast<const byte*>(nullptr), 0U, out, sizeof(out));
    EXPECT_EQ(0u, n);
}

TEST(SklBase64, encode_rfc4648_vectors_padded) {
    // RFC 4648 \S 10 test vectors
    struct v_t {
        const char* in;
        const char* out;
    };
    const v_t vectors[] = {
        {"",       ""        },
        {"f",      "Zg=="    },
        {"fo",     "Zm8="    },
        {"foo",    "Zm9v"    },
        {"foob",   "Zm9vYg=="},
        {"fooba",  "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
    };

    for (const auto& v : vectors) {
        char      buf[16] = {};
        const u64 in_len  = std::strlen(v.in);
        const u64 n       = skl::base64_encode<EBase64Alphabet::Standard, true, char>(v.in, in_len, buf, sizeof(buf));
        ASSERT_NE(CBase64Error, n);
        EXPECT_EQ(std::strlen(v.out), n);
        EXPECT_EQ(std::string(v.out), std::string(buf, n));
    }
}

TEST(SklBase64, encode_unpadded) {
    // "hello" -> "aGVsbG8=" padded, "aGVsbG8" unpadded
    char      buf[16] = {};
    const u64 n = skl::base64_encode<EBase64Alphabet::Standard, false, char>("hello", 5U, buf, sizeof(buf));
    ASSERT_NE(CBase64Error, n);
    EXPECT_EQ(7u, n);
    EXPECT_EQ(std::string("aGVsbG8"), std::string(buf, n));
}

TEST(SklBase64, encode_urlsafe_alphabet) {
    // 0xFB 0xFF 0xFE -> standard "+//+" -> urlsafe "-__-"
    const byte raw[] = {byte(0xFBu), byte(0xFFu), byte(0xFEu)};

    char      std_buf[8] = {};
    const u64 std_n      = skl::base64_encode<EBase64Alphabet::Standard>(raw, 3U, std_buf, sizeof(std_buf));
    ASSERT_EQ(4u, std_n);
    EXPECT_EQ(std::string("+//+"), std::string(std_buf, std_n));

    char      url_buf[8] = {};
    const u64 url_n      = skl::base64_encode<EBase64Alphabet::UrlSafe>(raw, 3U, url_buf, sizeof(url_buf));
    ASSERT_EQ(4u, url_n);
    EXPECT_EQ(std::string("-__-"), std::string(url_buf, url_n));
}

TEST(SklBase64, encode_insufficient_capacity_returns_error) {
    char buf[3] = {};
    // "foo" needs 4 chars padded, only 3 given
    const u64 n = skl::base64_encode<EBase64Alphabet::Standard, true, char>("foo", 3U, buf, sizeof(buf));
    EXPECT_EQ(CBase64Error, n);
}

// ============================================================================
// Runtime decode
// ============================================================================

TEST(SklBase64, decode_rfc4648_vectors_padded) {
    struct v_t {
        const char* in;
        const char* out;
    };
    const v_t vectors[] = {
        {"",         ""      },
        {"Zg==",     "f"     },
        {"Zm8=",     "fo"    },
        {"Zm9v",     "foo"   },
        {"Zm9vYg==", "foob"  },
        {"Zm9vYmE=", "fooba" },
        {"Zm9vYmFy", "foobar"},
    };

    for (const auto& v : vectors) {
        byte      buf[16] = {};
        const u64 in_len  = std::strlen(v.in);
        const u64 n       = skl::base64_decode<byte>(v.in, in_len, buf, sizeof(buf));
        ASSERT_NE(CBase64Error, n);
        EXPECT_EQ(std::strlen(v.out), n);
        EXPECT_EQ(0, std::memcmp(buf, v.out, n));
    }
}

TEST(SklBase64, decode_unpadded) {
    // "aGVsbG8" without trailing '='
    byte      buf[8] = {};
    const u64 n      = skl::base64_decode<byte>("aGVsbG8", 7U, buf, sizeof(buf));
    ASSERT_EQ(5u, n);
    EXPECT_EQ(0, std::memcmp(buf, "hello", 5));
}

TEST(SklBase64, decode_urlsafe_chars) {
    // "-__-" -> 0xFB 0xFF 0xFE (uses '-' and '_' from urlsafe alphabet)
    byte      buf[4] = {};
    const u64 n      = skl::base64_decode<byte>("-__-", 4U, buf, sizeof(buf));
    ASSERT_EQ(3u, n);
    EXPECT_EQ(byte(0xFBu), buf[0]);
    EXPECT_EQ(byte(0xFFu), buf[1]);
    EXPECT_EQ(byte(0xFEu), buf[2]);
}

TEST(SklBase64, decode_invalid_char_returns_error) {
    byte      buf[8] = {};
    const u64 n      = skl::base64_decode<byte>("!!!!", 4U, buf, sizeof(buf));
    EXPECT_EQ(CBase64Error, n);
}

TEST(SklBase64, decode_invalid_tail_length_returns_error) {
    // 5 chars -> tail == 1 which cannot produce any byte
    byte      buf[8] = {};
    const u64 n      = skl::base64_decode<byte>("Zm9vZ", 5U, buf, sizeof(buf));
    EXPECT_EQ(CBase64Error, n);
}

TEST(SklBase64, decode_insufficient_capacity_returns_error) {
    byte      buf[1] = {};
    const u64 n      = skl::base64_decode<byte>("Zm9v", 4U, buf, sizeof(buf));
    EXPECT_EQ(CBase64Error, n);
}

// ============================================================================
// Round trip
// ============================================================================

TEST(SklBase64, round_trip_all_byte_values) {
    byte input[256] = {};
    for (u32 i = 0U; i < 256U; ++i) {
        input[i] = static_cast<byte>(i);
    }

    char      enc[512] = {};
    const u64 enc_n    = skl::base64_encode<>(input, 256U, enc, sizeof(enc));
    ASSERT_NE(CBase64Error, enc_n);
    EXPECT_EQ(skl::base64_encoded_size(256U), enc_n);

    byte      dec[256] = {};
    const u64 dec_n    = skl::base64_decode<byte>(enc, enc_n, dec, sizeof(dec));
    ASSERT_EQ(256u, dec_n);
    EXPECT_EQ(0, std::memcmp(dec, input, 256));
}

// ============================================================================
// Compile-time wrappers
// ============================================================================

TEST(SklBase64, encode_str_c_compile_time) {
    constexpr auto E = skl::base64_encode_str_c("hello");
    static_assert(E.size() == 8U);
    static_assert(E.m_data[0] == 'a');
    static_assert(E.m_data[7] == '=');
    static_assert(E.m_data[8] == '\0');

    EXPECT_EQ(8u, E.size());
    EXPECT_STREQ("aGVsbG8=", E.c_str());
}

TEST(SklBase64, encode_str_c_empty) {
    constexpr auto E = skl::base64_encode_str_c("");
    static_assert(E.size() == 0U);
    static_assert(E.m_data[0] == '\0');
    EXPECT_EQ(0u, E.size());
}

TEST(SklBase64, encode_str_c_unpadded) {
    constexpr auto E = skl::base64_encode_str_c<6U, EBase64Alphabet::Standard, false>("hello");
    static_assert(E.size() == 7U);
    static_assert(E.m_data[7] == '\0');
    EXPECT_STREQ("aGVsbG8", E.c_str());
}

TEST(SklBase64, encode_c_byte_array_urlsafe) {
    constexpr byte raw[] = {byte(0xFBu), byte(0xFFu), byte(0xFEu)};
    constexpr auto E     = skl::base64_encode_c<3U, EBase64Alphabet::UrlSafe>(raw);
    static_assert(E.size() == 4U);
    static_assert(E.m_data[0] == '-');
    static_assert(E.m_data[1] == '_');
    static_assert(E.m_data[2] == '_');
    static_assert(E.m_data[3] == '-');
    EXPECT_STREQ("-__-", E.c_str());
}

TEST(SklBase64, decode_str_c_compile_time) {
    constexpr auto D = skl::base64_decode_str_c("aGVsbG8=");
    static_assert(D.is_valid());
    static_assert(D.size() == 5U);
    static_assert(D.m_data[0] == 'h');
    static_assert(D.m_data[4] == 'o');

    EXPECT_TRUE(D.is_valid());
    EXPECT_EQ(5u, D.size());
    EXPECT_EQ(0, std::memcmp(D.data(), "hello", 5));
}

TEST(SklBase64, decode_str_c_unpadded) {
    constexpr auto D = skl::base64_decode_str_c("aGVsbG8");
    static_assert(D.is_valid());
    static_assert(D.size() == 5U);
    EXPECT_EQ(0, std::memcmp(D.data(), "hello", 5));
}

TEST(SklBase64, decode_str_c_invalid) {
    constexpr auto D = skl::base64_decode_str_c("!!!!");
    static_assert(!D.is_valid());
    EXPECT_FALSE(D.is_valid());
}

// ============================================================================
// Thread-local scratch pad API
// ============================================================================

static_assert(skl::CBase64TlsScratchSize == 8192U);
static_assert(skl::CBase64TlsMaxInputSize == 6144U); // (8192 / 4) * 3

TEST(SklBase64Tls, encode_basic) {
    const skl::skl_string_view view = skl::base64_encode_tls("foobar", 6U);
    ASSERT_EQ(8u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "Zm9vYmFy", 8));
}

TEST(SklBase64Tls, encode_byte_overload) {
    const byte                  raw[]    = {byte(0xFBu), byte(0xFFu), byte(0xFEu)};
    const skl::skl_string_view  std_view = skl::base64_encode_tls(raw, 3U);
    ASSERT_EQ(4u, std_view.length());
    EXPECT_EQ(0, std::memcmp(std_view.data(), "+//+", 4));
}

TEST(SklBase64Tls, encode_urlsafe_unpadded) {
    const byte                 raw[]  = {byte(0xFBu), byte(0xFFu), byte(0xFEu)};
    const skl::skl_string_view view = skl::base64_encode_tls(raw, 3U, skl::EBase64Alphabet::UrlSafe, false);
    ASSERT_EQ(4u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "-__-", 4));
}

TEST(SklBase64Tls, encode_empty) {
    const skl::skl_string_view view = skl::base64_encode_tls("", 0U);
    EXPECT_EQ(0u, view.length());
}

TEST(SklBase64Tls, encode_exact_scratch_capacity) {
    // Input size 6144 bytes -> exactly 8192 encoded chars (padded) -> fits
    char input[skl::CBase64TlsMaxInputSize] = {};
    for (u64 i = 0U; i < skl::CBase64TlsMaxInputSize; ++i) {
        input[i] = static_cast<char>(i & 0xFFu);
    }
    const skl::skl_string_view view = skl::base64_encode_tls(input, skl::CBase64TlsMaxInputSize);
    EXPECT_EQ(skl::CBase64TlsScratchSize, view.length());
}

TEST(SklBase64Tls, encode_too_large_returns_empty) {
    // Input size 6145 bytes -> more than the scratch pad can hold when padded
    char input[skl::CBase64TlsMaxInputSize + 1U] = {};
    const skl::skl_string_view view = skl::base64_encode_tls(input, sizeof(input));
    EXPECT_EQ(0u, view.length());
    EXPECT_TRUE(view.empty());
}

TEST(SklBase64Tls, subsequent_call_overwrites_previous) {
    const skl::skl_string_view v1 = skl::base64_encode_tls("foo", 3U);
    ASSERT_EQ(4u, v1.length());
    EXPECT_EQ(0, std::memcmp(v1.data(), "Zm9v", 4));

    const skl::skl_string_view v2 = skl::base64_encode_tls("bar", 3U);
    ASSERT_EQ(4u, v2.length());
    EXPECT_EQ(0, std::memcmp(v2.data(), "YmFy", 4));

    // v1 and v2 point into the same scratch pad
    EXPECT_EQ(v1.data(), v2.data());
}

TEST(SklBase64Tls, scratch_accessor_reflects_last_encode) {
    (void)skl::base64_encode_tls("foobar", 6U);
    const skl::skl_string_view view = skl::base64_tls_scratch();
    ASSERT_EQ(8u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "Zm9vYmFy", 8));
}

TEST(SklBase64Tls, scratch_accessor_empty_after_failed_encode) {
    (void)skl::base64_encode_tls("foo", 3U);
    ASSERT_EQ(4u, skl::base64_tls_scratch().length());

    // Now fail with oversized input - scratch view should become empty
    char oversized[skl::CBase64TlsMaxInputSize + 1U] = {};
    (void)skl::base64_encode_tls(oversized, sizeof(oversized));
    EXPECT_EQ(0u, skl::base64_tls_scratch().length());
}

// ============================================================================
// Thread-local scratch pad - templated variants (compile-time dispatch)
// ============================================================================

TEST(SklBase64TlsTpl, default_template_args) {
    const skl::skl_string_view view = skl::base64_encode_tls<>("foobar", 6U);
    ASSERT_EQ(8u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "Zm9vYmFy", 8));
}

TEST(SklBase64TlsTpl, standard_padded) {
    const skl::skl_string_view view = skl::base64_encode_tls<skl::EBase64Alphabet::Standard, true>("Ma", 2U);
    ASSERT_EQ(4u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "TWE=", 4));
}

TEST(SklBase64TlsTpl, standard_unpadded) {
    const skl::skl_string_view view = skl::base64_encode_tls<skl::EBase64Alphabet::Standard, false>("hello", 5U);
    ASSERT_EQ(7u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "aGVsbG8", 7));
}

TEST(SklBase64TlsTpl, urlsafe_padded) {
    const byte                 raw[] = {byte(0xFBu), byte(0xFFu), byte(0xFEu)};
    const skl::skl_string_view view  = skl::base64_encode_tls<skl::EBase64Alphabet::UrlSafe, true>(raw, 3U);
    ASSERT_EQ(4u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "-__-", 4));
}

TEST(SklBase64TlsTpl, urlsafe_unpadded) {
    const byte                 raw[] = {byte(0xFBu)};
    const skl::skl_string_view view  = skl::base64_encode_tls<skl::EBase64Alphabet::UrlSafe, false>(raw, 1U);
    // 0xFB -> standard "+w", urlsafe "-w" (unpadded)
    ASSERT_EQ(2u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "-w", 2));
}

TEST(SklBase64TlsTpl, too_large_returns_empty) {
    char                       oversized[skl::CBase64TlsMaxInputSize + 1U] = {};
    const skl::skl_string_view view = skl::base64_encode_tls<>(oversized, sizeof(oversized));
    EXPECT_TRUE(view.empty());
}

TEST(SklBase64TlsTpl, updates_scratch_accessor) {
    (void)skl::base64_encode_tls<skl::EBase64Alphabet::UrlSafe>("foobar", 6U);
    const skl::skl_string_view view = skl::base64_tls_scratch();
    ASSERT_EQ(8u, view.length());
    EXPECT_EQ(0, std::memcmp(view.data(), "Zm9vYmFy", 8));
}

TEST(SklBase64Tls, thread_isolation) {
    // Two threads encoding concurrently must get independent scratch pads.
    // If the scratch pad were shared the result would frequently be corrupted.
    std::string t1_out;
    std::string t2_out;
    std::thread t1{[&t1_out]() {
        for (int i = 0; i < 1000; ++i) {
            const auto v = skl::base64_encode_tls("thread-1-data", 13U);
            t1_out.assign(v.data(), v.length());
        }
    }};
    std::thread t2{[&t2_out]() {
        for (int i = 0; i < 1000; ++i) {
            const auto v = skl::base64_encode_tls("thread-2-data", 13U);
            t2_out.assign(v.data(), v.length());
        }
    }};
    t1.join();
    t2.join();

    // Each thread's final snapshot must still match its own input exactly
    EXPECT_EQ(std::string("dGhyZWFkLTEtZGF0YQ=="), t1_out);
    EXPECT_EQ(std::string("dGhyZWFkLTItZGF0YQ=="), t2_out);
}

// ============================================================================
// Structural invariants
// ============================================================================

static_assert(skl::base64_encoded_t<8U>::CCapacity == 8U);
static_assert(sizeof(skl::base64_encoded_t<8U>::m_data) == 9U); // +1 for '\0'
static_assert(skl::base64_decoded_t<6U>::CCapacity == 6U);
