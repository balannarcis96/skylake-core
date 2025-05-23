//!
//! \file skl_slogger_bend
//!
//! \brief serialized logger back-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <fmt/core.h>
#include <fmt/args.h>

#include "skl_tls"
#include "skl_traits/size"
#include "skl_logger/skl_slogger_fend.hpp"
#include "skl_logger/skl_slogger_bend.hpp"

#if SKL_ENABLE_LOG_COLORS
#    define SKL_LOG_TRACE_ANSI_COLOR   "\x1b[37m"
#    define SKL_LOG_DEBUG_ANSI_COLOR   "\x1b[36m"
#    define SKL_LOG_INFO_ANSI_COLOR    "\x1b[35m"
#    define SKL_LOG_WARNING_ANSI_COLOR "\x1b[33m"
#    define SKL_LOG_ERROR_ANSI_COLOR   "\x1b[31m"
#    define SKL_LOG_FATAL_ANSI_COLOR   "\x1b[31m"
#    define SKL_LOG_ANSI_COLOR_END     "\x1b[0m"
#else
#    define SKL_LOG_TRACE_ANSI_COLOR   ""
#    define SKL_LOG_DEBUG_ANSI_COLOR   ""
#    define SKL_LOG_INFO_ANSI_COLOR    ""
#    define SKL_LOG_WARNING_ANSI_COLOR ""
#    define SKL_LOG_ERROR_ANSI_COLOR   ""
#    define SKL_LOG_FATAL_ANSI_COLOR   ""
#    define SKL_LOG_ANSI_COLOR_END     ""
#endif

#define SKL_LOG_DEBUG_TAG   "[DEBUG  ]"
#define SKL_LOG_INFO_TAG    "[INFO   ]"
#define SKL_LOG_WARNING_TAG "[WARNING]"
#define SKL_LOG_ERROR_TAG   "[ERROR  ]"
#define SKL_LOG_FATAL_TAG   "[FATAL  ]"
#define SKL_LOG_TRACE_TAG   "[TRACE  ]"

namespace {
struct DeserializeResult {
    u32                  m_timestamp   = 0U;
    u16                  m_uid         = 0;
    u16                  m_line_number = 0U;
    skl::skl_string_view m_file_name;
    skl::skl_string_view m_fmt_string;
    u16                  m_args_count = 0U;
    skl::skl_buffer_view m_args_buffer;
    skl::ELogType        m_type = skl::ELogType::ELogInfo;

    [[nodiscard]] bool is_fmt_string_only() const noexcept {
        return 0U == m_args_count;
    }

    [[nodiscard]] bool has_args() const noexcept {
        return 0U == m_args_count;
    }
};

[[nodiscard]] DeserializeResult deserilalize_log(skl::skl_stream& f_stream) noexcept {
    DeserializeResult result{};

    result.m_timestamp   = f_stream.read<u32>();
    result.m_uid         = f_stream.read<u16>();
    result.m_type        = static_cast<skl::ELogType>(f_stream.read<u8>());
    result.m_line_number = f_stream.read<u16>();
    result.m_file_name   = f_stream.read_length_prefixed_str_checked();
    result.m_fmt_string  = f_stream.read_length_prefixed_str_checked();
    result.m_args_count  = f_stream.read<u16>();

    //Until here we rely on the min check of the log buffer size to guarantee that we had space,
    //from now on we need to validate the read calls (99.9999% reads will be ok <-> very well predicted branches and will fit in buffer)
    result.m_args_buffer = f_stream.remaining_view();

    return result;
}

struct SLoggerBackEndTLS {
    byte                 g_bend_fmt_buffer[skl::CSerializedLoggerFrontEndBufferMinSize + 1U];
    skl::skl_buffer_view g_bend_fmt_buffer_view{skl::array_size(g_bend_fmt_buffer) - 1U, g_bend_fmt_buffer};
    char                 g_bend_output_buffer[(skl::CSerializedLoggerFrontEndBufferMinSize * 2U) + 1U];
};
} // namespace
SKL_MAKE_TLS_SINGLETON(SLoggerBackEndTLS, g_slogger_bend_tls);
namespace {
template <bool _AllowColors>
[[nodiscard]] skl::skl_string_view produce_fmt_string(const DeserializeResult& f_raw_log) noexcept {
    auto& backend = g_slogger_bend_tls::tls_guarded();

    char temp_buffer[32U];
    temp_buffer[31U] = 0U;

    auto& fmt_buffer = skl::skl_stream::make(backend.g_bend_fmt_buffer_view);
    fmt_buffer.reset();

#if SKL_ENABLE_LOG_COLORS
    constexpr bool CAllowColors = _AllowColors;
#else
    constexpr bool CAllowColors = false;
#endif

    //1. [TAG]
    switch (f_raw_log.m_type) {
        case skl::ELogType::ELogDebug:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_DEBUG_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_DEBUG_TAG);
            }
            break;
        case skl::ELogType::ELogInfo:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_INFO_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_INFO_TAG);
            }
            break;
        case skl::ELogType::ELogWarning:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_WARNING_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_WARNING_TAG);
            }
            break;
        case skl::ELogType::ELogError:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_ERROR_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_ERROR_TAG);
            }
            break;
        case skl::ELogType::ELogFatal:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_FATAL_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_FATAL_TAG);
            }
            break;
        case skl::ELogType::ELogTrace:
            {
                if constexpr (CAllowColors) {
                    fmt_buffer.write_unsafe(SKL_LOG_TRACE_ANSI_COLOR);
                    fmt_buffer.seek_backward(1U); //ignore null-terminator
                }
                fmt_buffer.write_unsafe(SKL_LOG_TRACE_TAG);
            }
            break;
        default:
            {
                fmt_buffer.write_unsafe("[SLOGGER] UNKNOWN TYPE RECEIVED!");
            }
            break;
    }
    fmt_buffer.seek_backward(1U); //ignore null-terminator

    //2. [TIMESTAMP]
    fmt_buffer.write<byte>(byte('['));

    const auto hours        = u32(f_raw_log.m_timestamp / 3600000);
    const auto minutes      = u32((f_raw_log.m_timestamp - (hours * 3600000)) / 60000);
    const auto seconds      = u32((f_raw_log.m_timestamp - (hours * 3600000) - (minutes * 60000)) / 1000);
    const auto milliseconds = u32(f_raw_log.m_timestamp - (hours * 3600000) - (minutes * 60000) - (seconds * 1000));

    auto temp_len = snprintf(temp_buffer,
                             skl::array_size(temp_buffer) - 1U,
                             "%02u:%02u:%02u.%03u",
                             hours,
                             minutes,
                             seconds,
                             milliseconds);
    fmt_buffer.write_unsafe(temp_buffer, temp_len); //timestamp_len does not count the null-terminator
    fmt_buffer.write<byte>(byte(']'));

    //3. [ID]
    fmt_buffer.write<byte>(byte('['));
    temp_len = snprintf(temp_buffer, skl::array_size(temp_buffer) - 1U, "%d", i32(f_raw_log.m_uid));
    fmt_buffer.write_unsafe(temp_buffer, temp_len); //timestamp_len does not count the null-terminator
    fmt_buffer.write<byte>(byte(']'));

    //4. [FILE:LINE_NUMBER]
    fmt_buffer.write<byte>(byte('['));
    fmt_buffer.write_unsafe(f_raw_log.m_file_name.data(), f_raw_log.m_file_name.length());
    fmt_buffer.write<byte>(byte(':'));
    temp_len = snprintf(temp_buffer, skl::array_size(temp_buffer) - 1U, "%d", i32(f_raw_log.m_line_number));
    fmt_buffer.write_unsafe(temp_buffer, temp_len); //timestamp_len does not count the null-terminator
    fmt_buffer.write<byte>(byte(']'));

    //Delimiter
    fmt_buffer.write<byte>(byte(' '));
    if (skl::ELogType::ELogTrace != f_raw_log.m_type) {
        fmt_buffer.write<byte>(byte('-'));
        fmt_buffer.write<byte>(byte('-'));
        fmt_buffer.write<byte>(byte(' '));
    }

    //5. FMT_STR
    fmt_buffer.write_unsafe(f_raw_log.m_fmt_string.data(), f_raw_log.m_fmt_string.length());

    //6.END
    if constexpr (CAllowColors) {
        fmt_buffer.write_unsafe(SKL_LOG_ANSI_COLOR_END);
    } else {
        fmt_buffer.write<byte>(0U); //null-terminator
    }

    SKL_ASSERT_CRITICAL((0U < fmt_buffer.position()) && (fmt_buffer.position() < fmt_buffer.length()));

    //Return as string view (including the null-terminator)
    return skl::skl_string_view::exact(reinterpret_cast<const char*>(fmt_buffer.buffer()), fmt_buffer.position());
}

template <bool _AllowColors>
skl::skl_string_view slogger_backend_process(skl::skl_stream& f_stream) noexcept {
    auto  result      = deserilalize_log(f_stream);
    auto  fmt_str     = produce_fmt_string<_AllowColors>(result);
    auto& args_stream = skl::skl_stream::make(result.m_args_buffer);

    auto& backend = g_slogger_bend_tls::tls_checked();

    if (result.is_fmt_string_only()) {
        return fmt_str;
    }

    fmt::dynamic_format_arg_store<fmt::format_context> args{};
    args.reserve(result.m_args_count, 0);

    for (u32 i = 0U; i < result.m_args_count; ++i) {
        const auto arg_type = skl::ELogParamType{args_stream.try_read<byte>(skl::ELogParamType::None)};
        switch (arg_type) {
            case skl::ELogParamType::EInt8:
                {
                    args.push_back(args_stream.try_read<i8>(0));
                }
                break;
            case skl::ELogParamType::EUInt8:
                {
                    args.push_back(args_stream.try_read<u8>(0));
                }
                break;
            case skl::ELogParamType::EInt16:
                {
                    args.push_back(args_stream.try_read<i16>(0));
                }
                break;
            case skl::ELogParamType::EUInt16:
                {
                    args.push_back(args_stream.try_read<u16>(0));
                }
                break;
            case skl::ELogParamType::EInt32:
                {
                    args.push_back(args_stream.try_read<i32>(0));
                }
                break;
            case skl::ELogParamType::EUInt32:
                {
                    args.push_back(args_stream.try_read<u32>(0));
                }
                break;
            case skl::ELogParamType::EInt64:
                {
                    args.push_back(args_stream.try_read<i64>(0));
                }
                break;
            case skl::ELogParamType::EUInt64:
                {
                    args.push_back(args_stream.try_read<u64>(0));
                }
                break;
            case skl::ELogParamType::EFloat:
                {
                    args.push_back(args_stream.try_read<float>(0.0f));
                }
                break;
            case skl::ELogParamType::EDouble:
                {
                    args.push_back(args_stream.try_read<double>(0.0));
                }
                break;
            case skl::ELogParamType::EStringView:
                [[fallthrough]];
            case skl::ELogParamType::EString:
                {
                    auto str = args_stream.read_length_prefixed_str();
                    if (str.is_success()) {
                        args.push_back(str.value().std<std::string_view>());
                    }
                }
                break;
            default:
            case skl::ELogParamType::None:
                {
                    skl::skl_buffer_view temp{skl::array_size(backend.g_bend_output_buffer) - 1U, reinterpret_cast<byte*>(backend.g_bend_output_buffer)};
                    auto&                temp_stream = skl::skl_stream::make(temp);
                    temp_stream.write_unsafe("[SLogger] -- UNKNOWN ARG TYPE!");
                    return skl::skl_string_view::exact(backend.g_bend_output_buffer, temp_stream.position() - 1U);
                }
        }
    }

    const auto fmt_result = fmt::vformat_to_n(backend.g_bend_output_buffer,
                                              skl::array_size(backend.g_bend_output_buffer) - 1U,
                                              fmt::string_view{fmt_str.data(), fmt_str.length()},
                                              args);

    return skl::skl_string_view::exact(backend.g_bend_output_buffer, fmt_result.size);
}

} // namespace

namespace skl {
void SKLSerializedLoggerBackend::prepare_thread_log_buffer(skl_stream& f_stream) noexcept {
}

skl_string_view SKLSerializedLoggerBackend::process(skl_stream& f_stream) noexcept {
    return slogger_backend_process<true>(f_stream);
}

skl_string_view SKLSerializedLoggerBackend::process_no_colors(skl_stream& f_stream) noexcept {
    return slogger_backend_process<false>(f_stream);
}
} // namespace skl

namespace skl {
void skl_core_init_thread__slog_bend() noexcept {
    SKL_ASSERT_PERMANENT(g_slogger_bend_tls::tls_create().is_success());
}
void skl_core_deinit_thread__slog_bend() noexcept {
    g_slogger_bend_tls::tls_destroy();
}
} // namespace skl
