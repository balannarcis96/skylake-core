//!
//! \file skl_slogger_bend
//!
//! \brief Serialized logger back-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

#include "skl_stream"
#include "skl_string_view"

namespace skl {
struct SKLSerializedLoggerBackend {
    SKLSerializedLoggerBackend() noexcept                                    = delete;
    ~SKLSerializedLoggerBackend() noexcept                                   = delete;
    SKLSerializedLoggerBackend(const SKLSerializedLoggerBackend&)            = delete;
    SKLSerializedLoggerBackend& operator=(const SKLSerializedLoggerBackend&) = delete;
    SKLSerializedLoggerBackend(SKLSerializedLoggerBackend&&)                 = delete;
    SKLSerializedLoggerBackend& operator=(SKLSerializedLoggerBackend&&)      = delete;

    static void            prepare_thread_log_buffer(skl_stream& f_stream) noexcept;
    static skl_string_view process(skl_stream& f_stream) noexcept;
    static skl_string_view process_no_colors(skl_stream& f_stream) noexcept;
};
} // namespace skl
