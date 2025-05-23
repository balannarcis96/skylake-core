//!
//! \file skl_stream
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstring>
#include <fstream>

#include "skl_stream"

namespace skl {
pair<u32, bool> skl_stream::count_non_zero() const noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    const auto* begin = front();
    u32         start = position();
    u32         end   = length();
    u32         count = 0U;
    while ((start + count) < end) [[likely]] {
        const auto current_char = begin[count];
        if (0 == current_char) {
            [[unlikely]] break;
        }
        ++count;
    }

    return {.first = count, .second = ((start + count) < end)};
}

pair<u32, bool> skl_stream::skip_cstring() noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    const auto* begin        = front();
    u32         start        = position();
    u32         end          = length();
    u32         count        = 0U;
    byte        current_byte = 0xFFU;
    while ((start + count) < end) [[likely]] {
        current_byte = begin[count++];
        if (0U == current_byte) {
            [[unlikely]] break;
        }
    }

    seek_forward(count);
    return {.first = count, .second = current_byte != 0U};
}

skl_result<skl_string_view> skl_stream::read_length_prefixed_str() noexcept {
    if (remaining() < sizeof(str_len_prefix_t)) {
        return skl_fail{SKL_ERR_SIZE};
    }

    const auto length = read<str_len_prefix_t>();
    if (length > remaining()) {
        seek_backward(sizeof(str_len_prefix_t));
        return skl_fail{SKL_ERR_CORRUPT};
    }

    const auto* str = c_str();
    seek_forward(length);
    return skl_string_view::exact(str, length);
}

skl_string_view skl_stream::read_length_prefixed_str_checked() noexcept {
    SKL_ASSERT_CRITICAL(remaining() >= sizeof(str_len_prefix_t));
    const auto length = read<str_len_prefix_t>();
    SKL_ASSERT_CRITICAL(length <= remaining());
    const auto* str = c_str();
    seek_forward(length);
    return skl_string_view::exact(str, length);
}

bool skl_stream::write(const byte* f_source, u32 f_source_size) noexcept {
    SKL_ASSERT(nullptr != f_source);
    SKL_ASSERT(0U < f_source_size);
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    const auto space = remaining();
    if (f_source_size <= space) {
        (void)std::memcpy(front(), f_source, f_source_size);
        seek_forward(f_source_size);
        [[likely]] return true;
    }

    return false;
}

void skl_stream::write_unsafe(const byte* f_source, u32 f_source_size) noexcept {
    SKL_ASSERT(nullptr != f_source);
    SKL_ASSERT(0U < f_source_size);
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    SKL_ASSERT_CRITICAL(fits(f_source_size));
    (void)std::memcpy(front(), f_source, f_source_size);
    seek_forward(f_source_size);
}

skl_status skl_stream::write_length_prefixed_str(skl_string_view f_string) noexcept {
    if ((f_string.length() + sizeof(str_len_prefix_t)) > remaining()) {
        return SKL_ERR_SIZE;
    }

    //Write the length prefix
    write<str_len_prefix_t>(f_string.length());

    //Write string
    if (false == f_string.empty()) [[likely]] {
        write_unsafe(f_string.data(), f_string.length());
    }

    [[likely]] return SKL_SUCCESS;
}

void skl_stream::write_length_prefixed_str_checked(skl_string_view f_string) noexcept {
    SKL_ASSERT_CRITICAL((f_string.length() + sizeof(str_len_prefix_t)) <= remaining());

    //Write the length prefix
    write<str_len_prefix_t>(f_string.length());

    //Write string
    if (false == f_string.empty()) [[likely]] {
        write_unsafe(f_string.data(), f_string.length());
    }
}

skl_status skl_stream::write_cstr(skl_string_view f_string) noexcept {
    if ((f_string.length() + sizeof(char)) > remaining()) {
        return SKL_ERR_SIZE;
    }

    if (false == f_string.empty()) [[likely]] {
        write_unsafe(f_string.data(), f_string.length());
    }

    //Write null-terminator
    write<char>(0);

    [[likely]] return SKL_SUCCESS;
}

void skl_stream::write_cstr_checked(skl_string_view f_string) noexcept {
    SKL_ASSERT_CRITICAL((f_string.length() + sizeof(char)) <= remaining());

    if (false == f_string.empty()) [[likely]] {
        write_unsafe(f_string.data(), f_string.length());
    }

    //Write null-terminator
    write<char>(0);
}

void skl_stream::zero() noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    (void)memset(buffer(), 0, length());
}

void skl_stream::zero_remaining() noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < remaining());

    (void)memset(front(), 0, remaining());
}

skl_status skl_stream::read_from_file(const char* f_file_name) noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    auto file = std::ifstream(f_file_name, std::ifstream::binary);
    if (false == file.is_open()) {
        return SKL_ERR_FILE;
    }

    file.seekg(0, std::ifstream::end);
    const u32 file_size{static_cast<u32>(file.tellg())};
    file.seekg(0, std::ifstream::beg);

    if (0U == file_size) {
        file.close();
        return SKL_ERR_EMPTY;
    }

    if (false == fits(file_size)) {
        file.close();
        return SKL_ERR_TRUN;
    }

    (void)file.read(reinterpret_cast<char*>(front()), file_size);

    //Check if the read went successfully
    if (false == file.good()) {
        file.close();
        return SKL_ERR_READ;
    }

    file.close();
    seek_forward(file_size);
    [[likely]] return SKL_SUCCESS;
}

skl_status skl_stream::read_from_text_file(const char* f_file_name) noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    auto file = std::ifstream(f_file_name, std::ifstream::binary);
    if (false == file.is_open()) {
        return SKL_ERR_FILE;
    }

    file.seekg(0, std::ifstream::end);
    const u32 file_size{static_cast<u32>(file.tellg())};
    file.seekg(0, std::ifstream::beg);

    if (0U == file_size) {
        file.close();
        return SKL_ERR_EMPTY;
    }

    //Account for the null-terminator too
    if (false == fits(file_size + 1U)) {
        file.close();
        return SKL_ERR_TRUN;
    }

    (void)file.read(reinterpret_cast<char*>(front()), file_size);

    //Check if the read went successfully
    if (false == file.good()) {
        file.close();
        return SKL_ERR_READ;
    }

    file.close();

    //Optimistic approach: advance the position
    seek_forward(file_size);

    //Add null-terminator
    *front() = '\0';
    seek_forward(1U);

    [[likely]] return SKL_SUCCESS;
}

bool skl_stream::read(byte* f_dest, u32 f_read_size) noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    if (remaining() < f_read_size) {
        return false;
    }

    (void)memcpy(reinterpret_cast<void*>(f_dest),
                 reinterpret_cast<const void*>(front()),
                 f_read_size);

    seek_forward(f_read_size);
    return true;
}

skl_status skl_stream::write_to_file(const char* f_file_name) const noexcept {
    SKL_ASSERT(nullptr != buffer());
    SKL_ASSERT(0U < length());

    if (0U == remaining()) {
        return SKL_ERR_EMPTY;
    }

    auto file = std::ofstream(f_file_name, std::ifstream::binary | std::ifstream::trunc);
    if (false == file.is_open()) {
        return SKL_ERR_FILE;
    }

    (void)file.write(reinterpret_cast<const char*>(front()), remaining());

    //Check if the write went successfully
    if (false == file.good()) {
        file.close();
        return SKL_ERR_READ;
    }

    file.close();
    return SKL_SUCCESS;
}

} // namespace skl
