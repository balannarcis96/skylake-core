//!
//! \file skl_base64
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_base64"

namespace skl {
namespace {
    thread_local char g_base64_tls_scratch[CBase64TlsScratchSize];
    thread_local u64  g_base64_tls_scratch_length = 0U;
} // namespace

namespace base64_detail {
    char* tls_scratch_ptr() noexcept {
        return g_base64_tls_scratch;
    }

    void tls_scratch_set_length(u64 f_length) noexcept {
        g_base64_tls_scratch_length = f_length;
    }
} // namespace base64_detail

skl_string_view base64_tls_scratch() noexcept {
    return skl_string_view::exact(g_base64_tls_scratch, g_base64_tls_scratch_length);
}

skl_string_view base64_encode_tls(const byte* f_input, u64 f_input_size, EBase64Alphabet f_alphabet, bool f_padded) noexcept {
    u64 written = CBase64Error;

    if (EBase64Alphabet::UrlSafe == f_alphabet) {
        if (f_padded) {
            written = base64_encode<EBase64Alphabet::UrlSafe, true, byte>(f_input, f_input_size, g_base64_tls_scratch, CBase64TlsScratchSize);
        } else {
            written = base64_encode<EBase64Alphabet::UrlSafe, false, byte>(f_input, f_input_size, g_base64_tls_scratch, CBase64TlsScratchSize);
        }
    } else {
        if (f_padded) {
            written = base64_encode<EBase64Alphabet::Standard, true, byte>(f_input, f_input_size, g_base64_tls_scratch, CBase64TlsScratchSize);
        } else {
            written = base64_encode<EBase64Alphabet::Standard, false, byte>(f_input, f_input_size, g_base64_tls_scratch, CBase64TlsScratchSize);
        }
    }

    if (CBase64Error == written) {
        g_base64_tls_scratch_length = 0U;
        return skl_string_view{};
    }
    g_base64_tls_scratch_length = written;
    return skl_string_view::exact(g_base64_tls_scratch, written);
}

skl_string_view base64_encode_tls(const char* f_input, u64 f_input_size, EBase64Alphabet f_alphabet, bool f_padded) noexcept {
    return base64_encode_tls(reinterpret_cast<const byte*>(f_input), f_input_size, f_alphabet, f_padded);
}
} // namespace skl
