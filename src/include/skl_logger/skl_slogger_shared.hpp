//!
//! \file skl_slogger_shared
//!
//! \brief Serialized logger shared
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

#include "skl_int"

namespace skl {
using slogger_sink_id_t = int;

enum ELogType : unsigned char {
    ELogTrace,
    ELogDebug,
    ELogInfo,
    ELogWarning,
    ELogError,
    ELogFatal
};

enum class ESLoggerSinkType {
    Network,
    FileHandle, //Default(stdout)
    File,
    Custom,

    MAX
};

constexpr slogger_sink_id_t CSLoggerInvlidSinkId     = -1;
constexpr slogger_sink_id_t CSLoggerNetSinkId        = slogger_sink_id_t(ESLoggerSinkType::Network);
constexpr slogger_sink_id_t CSLoggerFileHandleSinkId = slogger_sink_id_t(ESLoggerSinkType::FileHandle);
constexpr slogger_sink_id_t CSLoggerFileSinkId       = slogger_sink_id_t(ESLoggerSinkType::File);
constexpr slogger_sink_id_t CSLoggerCustomSink       = slogger_sink_id_t(ESLoggerSinkType::Custom);

//! Log level mask bits (0 = all logging disabled)
constexpr u32 CSLoggerLevelFatal   = 1U << 0U;
constexpr u32 CSLoggerLevelError   = 1U << 1U;
constexpr u32 CSLoggerLevelWarning = 1U << 2U;
constexpr u32 CSLoggerLevelInfo    = 1U << 3U;
constexpr u32 CSLoggerLevelDebug   = 1U << 4U;
constexpr u32 CSLoggerLevelTrace   = 1U << 5U;

//! Log level presets
constexpr u32 CSLoggerLevelNone     = 0U;
constexpr u32 CSLoggerLevelShipping = CSLoggerLevelFatal | CSLoggerLevelError;
constexpr u32 CSLoggerLevelStaging  = CSLoggerLevelShipping | CSLoggerLevelWarning | CSLoggerLevelInfo;
constexpr u32 CSLoggerLevelDev      = CSLoggerLevelStaging | CSLoggerLevelDebug | CSLoggerLevelTrace;
constexpr u32 CSLoggerLevelAll      = CSLoggerLevelDev;

//! Get the current log level mask
[[nodiscard]] u32 skl_get_log_level_mask() noexcept;

//! Set the current log level mask
void skl_set_log_level_mask(u32 f_mask) noexcept;
} // namespace skl
