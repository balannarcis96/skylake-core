//!
//! \file skl_slogger_shared
//!
//! \brief Serialized logger shared
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

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
} // namespace skl
