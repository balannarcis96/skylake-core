//!
//! \file skl_slogger_fend
//!
//! \brief Linux x86-x64 - serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
//! \copyright Copyright (c) 2025 Balan Narcis. All rights reserved.
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

#include "skl_stream"
#include "skl_string_view"
#include "skl_traits/is_cstring"
#include "skl_traits/rm_all_cv"

namespace skl::skl_log_internal {

constexpr u64 CMaxSerializedLoggerHeaderSize         = 256U;
constexpr u64 CSerializedLoggerFrontEndBufferMinSize = 4096U;

constexpr u64 CSerializedLoggerFixedHeaderSize = 4U  //Timestamp (relative to the start of the program)
                                               + 2U  //UID (thread specific unique identifier | unique to the current program instance only)
                                               + 1U  //Type
                                               + 2U  //Line Number
                                               + 2U; //Params Count

static_assert(CMaxSerializedLoggerHeaderSize >= CSerializedLoggerFixedHeaderSize);

enum ELogType : u8 {
    ELogTrace,
    ELogDebug,
    ELogInfo,
    ELogWarning,
    ELogError,
    ELogFatal
};

enum ELogParamType : u8 {
    None,
    EInt8,
    EInt16,
    EInt32,
    EInt64,
    EUInt8,
    EUInt16,
    EUInt32,
    EUInt64,
    EFloat,
    EDouble,
    EString
};

[[nodiscard]] skl_stream& skl_begin_log() noexcept;
void                      skl_commit_log() noexcept;

struct SKLSerializedLoggerFrontEnd {
    SKLSerializedLoggerFrontEnd() noexcept                                     = delete;
    ~SKLSerializedLoggerFrontEnd() noexcept                                    = delete;
    SKLSerializedLoggerFrontEnd(const SKLSerializedLoggerFrontEnd&)            = delete;
    SKLSerializedLoggerFrontEnd& operator=(const SKLSerializedLoggerFrontEnd&) = delete;
    SKLSerializedLoggerFrontEnd(SKLSerializedLoggerFrontEnd&&)                 = delete;
    SKLSerializedLoggerFrontEnd& operator=(SKLSerializedLoggerFrontEnd&&)      = delete;

    //! Submit new log
    template <ELogType _Type, u64 _FileNameLength, u16 _LineNumber, u64 _FormatStringSize, typename... _Args>
    static void log(skl_string_view f_file_name, const char (&f_fmt)[_FormatStringSize], _Args... f_args) noexcept {
        //Check that the Fixed hader, file name and fmt string can fit in the serialized logging front end buffer
        constexpr u64 _MinRequiredSize = CSerializedLoggerFixedHeaderSize + _FileNameLength + _FormatStringSize;
        static_assert(CSerializedLoggerFrontEndBufferMinSize >= _MinRequiredSize);

        auto& log_buffer = skl_begin_log();
        if (serialize<_Type>(log_buffer, _LineNumber, f_file_name, f_fmt, f_args...)) [[likely]] {
            [[likely]] skl_commit_log();
        }
    }

private:
    //! Serialize the raw log data into the given stream if
    //! \note Protocol: Skylake Protocol
    template <ELogType _Type, u64 _FormatStringSize, typename... _Args>
    [[nodiscard]] static bool serialize(skl_stream& f_stream, u16 f_line_number, skl_string_view f_file_name, const char (&f_fmt)[_FormatStringSize], _Args... f_args) noexcept {
        //3. Log Type (We are guaranteed to have space for the type)
        f_stream.write<u8>(u8(_Type));

        //4. Line number (We are guaranteed to have space for the line number)
        f_stream.write<u16>(f_line_number);

        //5. File name (We are guaranteed to have space for the file name string)
        f_stream.write_unsafe(reinterpret_cast<const byte*>(f_file_name.data()), f_file_name.length());

        //6. Format string (We are guaranteed to have space for the fmt string)
        f_stream.write_unsafe(f_fmt);

        //7. Write Params if any (We are guaranteed to have space for the params count)
        f_stream.write<u16>(static_cast<u16>(sizeof...(_Args)));

        //Until here we rely on the min check of the log buffer size to guarantee that we had space,
        //from now on we need to validate the write calls (99.9999% they will be ok = very well predicted branches)

        if constexpr (sizeof...(_Args) > 0U) {
            if (false == serialize_args(f_stream, f_args...)) {
                [[unlikely]] return false;
            }
        }

        [[likely]] return true;
    }

    //! Serialize the arguments into the given stream if
    template <typename... _Args>
    [[nodiscard]] static constexpr bool serialize_args(skl_stream& f_stream, _Args... f_args) noexcept {
        if constexpr (0U == sizeof...(_Args)) {
            return true;
        } else {
            return serialize_args_internal(f_stream, f_args...);
        }
    }

    template <typename _Arg, typename... _Args>
    [[nodiscard]] static constexpr bool serialize_args_internal(skl_stream& f_stream, _Arg f_arg, _Args... f_args) noexcept {
        //Accept only utf-8 strings
        static_assert(false == is_cstring<_Arg, u16>());
        static_assert(false == is_cstring<_Arg, u32>());
        static_assert(false == is_cstring<_Arg, wchar_t>());

        constexpr bool          b_is_string = is_cstring<_Arg, char>();
        constexpr ELogParamType arg_type    = get_arg_type<_Arg>();

        static_assert(ELogParamType::None != arg_type, "Unsupported argument type for serialized logging!");

        //1. Write type
        if (false == f_stream.write_safe<u8>(static_cast<u8>(arg_type))) {
            SKL_ASSERT(false);
            [[unlikely]] return false;
        }

        //2. Write value
        if constexpr (b_is_string) {
            const auto result = f_stream.write_str(f_arg);
            SKL_ASSERT(result);
            if (false == result) {
                [[unlikely]] return false;
            }
        } else {
            const auto result = f_stream.write_safe<typename full_cv_helper<_Arg>::raw_type>(f_arg);
            SKL_ASSERT(result);
            if (false == result) {
                [[unlikely]] return false;
            }
        }

        if constexpr (sizeof...(f_args) > 0U) {
            //Next arg if any
            [[likely]] return serialize_args_internal(f_stream, f_args...);
        } else {
            [[likely]] return true;
        }
    }

    //! Get the argument type
    //! \note 100% Compile time
    template <typename _Arg>
    [[nodiscard]] static consteval ELogParamType get_arg_type() noexcept {
        using _RawArgType = full_cv_helper<_Arg>::type;

        if constexpr (is_cstring<_Arg, char>()) {
            return ELogParamType::EString;
        } else if constexpr (__is_same(_RawArgType, i8)) {
            return ELogParamType::EInt8;
        } else if constexpr (__is_same(_RawArgType, u8)) {
            return ELogParamType::EUInt8;
        } else if constexpr (__is_same(_RawArgType, i16)) {
            return ELogParamType::EInt16;
        } else if constexpr (__is_same(_RawArgType, u16)) {
            return ELogParamType::EUInt16;
        } else if constexpr (__is_same(_RawArgType, i32)) {
            return ELogParamType::EInt32;
        } else if constexpr (__is_same(_RawArgType, u32)) {
            return ELogParamType::EUInt32;
        } else if constexpr (__is_same(_RawArgType, i64)) {
            return ELogParamType::EInt64;
        } else if constexpr (__is_same(_RawArgType, u64)) {
            return ELogParamType::EUInt64;
        } else if constexpr (__is_same(_RawArgType, float)) {
            return ELogParamType::EFloat;
        } else if constexpr (__is_same(_RawArgType, double)) {
            return ELogParamType::EDouble;
        } else {
            return ELogParamType::None;
        }
    }
};

} // namespace skl::skl_log_internal

namespace skl {

//! Submit a serialized log message
template <skl_log_internal::ELogType _Type, u64 _FileNameLength, u16 _LineNumber, u64 _FormatStringSize, typename... _Args>
[[clang::noinline]] static void skl_log(skl_string_view f_file_name, const char (&f_fmt)[_FormatStringSize], _Args... f_args) noexcept {
    skl_log_internal::SKLSerializedLoggerFrontEnd::log<_Type, _FileNameLength, _LineNumber>(f_file_name, f_fmt, f_args...);
}

} // namespace skl
