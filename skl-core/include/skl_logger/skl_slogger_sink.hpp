//!
//! \file skl_slogger_sink
//!
//! \brief Linux x86-x64 - serialized logger sink
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

#include "skl_result"

namespace skl {
struct skl_stream;

enum class ESLoggerSinkType {
    Network,
    FileHandle, //Default(stdout)
    File,
    Custom
};

//! Custom logger sink interface
struct ISloggerSink {
    virtual ~ISloggerSink() = default;

    //!
    virtual void thread_init(skl_stream&) noexcept   = 0;
    virtual void thread_deinit(skl_stream&) noexcept = 0;

    //! Begin the given \p f_log_stream
    virtual void begin_log(skl_stream& f_log_stream) noexcept = 0;

    //! End the given \p f_log_stream
    virtual void end_log(skl_stream& f_log_stream) noexcept = 0;

    //! Sinkt the given \p f_log_stream
    virtual void sink_log(skl_stream& f_log_stream) noexcept = 0;
};

//! File logger sink config
struct SLoggerFileSinkConfig {
};

//! Network logger sink config
struct SLoggerNetworkSinkConfig {
};

struct SLoggerSink {
    using SinkId = i32;

    SLoggerSink() noexcept                     = delete;
    ~SLoggerSink() noexcept                    = delete;
    SLoggerSink(const SLoggerSink&)            = delete;
    SLoggerSink& operator=(const SLoggerSink&) = delete;
    SLoggerSink(SLoggerSink&&)                 = delete;
    SLoggerSink& operator=(SLoggerSink&&)      = delete;

public:
    //! Init the sink manager on the calling thread
    [[nodiscard]] static skl_status init_thread() noexcept;

    //! Deinit the sink manager on the calling thread
    static void deinit_thread() noexcept;

    //! Setup new custom sink
    [[nodiscard]] static skl_result<SinkId> setup_custom_sink(ISloggerSink* f_sink) noexcept;

    //! Setup new network sink
    [[nodiscard]] static skl_result<SinkId> setup_network_sink(const SLoggerNetworkSinkConfig& f_config) noexcept;

    //! Setup new file sink
    [[nodiscard]] static skl_result<SinkId> setup_file_sink(const SLoggerFileSinkConfig& f_config) noexcept;

    //! Setup new file handle sink
    [[nodiscard]] static skl_result<SinkId> setup_handle_sink(void* f_file_handle) noexcept;

    //! Set the current sink
    [[nodiscard]] static skl_status set_current_sink(SinkId f_id) noexcept;

public:
    //! Create and prepare new log buffer
    static skl_stream& begin_log() noexcept;

    //! Sink the current log buffer
    static void sink_log() noexcept;
};

} // namespace skl
