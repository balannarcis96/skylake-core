//!
//! \file skl_slogger_sink
//!
//! \brief Serialized logger sink
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#pragma once

#include "skl_status"
#include "skl_assert"
#include "skl_logger/skl_slogger_shared.hpp"

namespace skl {
struct skl_stream;
struct LoggerFileHandleSink;

//! Custom logger sink base
struct SLoggerSink {
    SLoggerSink(bool f_has_begin) noexcept
        : m_id(CSLoggerCustomSink)
        , m_has_begin(f_has_begin) { }

    SLoggerSink(const SLoggerSink&) noexcept            = delete;
    SLoggerSink(SLoggerSink&&) noexcept                 = delete;
    SLoggerSink& operator=(const SLoggerSink&) noexcept = delete;
    SLoggerSink& operator=(SLoggerSink&&) noexcept      = delete;

    virtual ~SLoggerSink() = default;

    //! Init the sink on the calling thread
    virtual void thread_init() noexcept = 0;

    //! Deinit the sink on the calling thread
    virtual void thread_deinit() noexcept = 0;

    //! Begin the given \p f_log_stream
    virtual void begin_log(skl_stream& f_log_stream) noexcept = 0;

    //! Sinkt the given \p f_log_stream
    virtual void end_and_sink_log(skl_stream& f_log_stream) noexcept = 0;

    //! Get logger sink id
    [[nodiscard]] slogger_sink_id_t id() const noexcept {
        return m_id;
    }

    //! Does this sink require begin_log(...) call?
    [[nodiscard]] bool has_begin() const noexcept {
        return m_has_begin;
    }

private:
    SLoggerSink(bool f_has_begin, slogger_sink_id_t f_id) noexcept
        : m_id(f_id)
        , m_has_begin(f_has_begin) {
        SKL_ASSERT_PERMANENT((f_id >= CSLoggerNetSinkId) && (f_id <= CSLoggerCustomSink));
    }

private:
    const slogger_sink_id_t m_id;        //!< Sink id
    const bool              m_has_begin; //!< Requires begin_log() called

    friend LoggerFileHandleSink;
};

//! File logger sink config
struct slogger_file_sink_config_t {
};

//! Network logger sink config
struct slogger_net_sink_config_t {
};

//! Manage SLogger sinks
struct SLoggerSinkManager {
    SLoggerSinkManager() noexcept                            = delete;
    ~SLoggerSinkManager() noexcept                           = delete;
    SLoggerSinkManager(const SLoggerSinkManager&)            = delete;
    SLoggerSinkManager& operator=(const SLoggerSinkManager&) = delete;
    SLoggerSinkManager(SLoggerSinkManager&&)                 = delete;
    SLoggerSinkManager& operator=(SLoggerSinkManager&&)      = delete;

    //! Prepare the sink on the calling thread
    [[nodiscard]] static skl_status init_thread() noexcept;

    //! Deinit the sink on the calling thread
    static void deinit_thread() noexcept;

    //! Setup new custom sink
    //! \returns SKL_ERR_PARAMS if f_sink is nullptr
    [[nodiscard]] static skl_status register_custom_sink(SLoggerSink* f_sink) noexcept;

    //! Setup the network sink
    [[nodiscard]] static skl_status setup_network_sink(const slogger_net_sink_config_t& f_config) noexcept;

    //! Setup the file sink
    [[nodiscard]] static skl_status setup_file_sink(const slogger_file_sink_config_t& f_config) noexcept;

    //! Setup the file handle sink
    //! \returns SKL_ERR_PARAMS if \p f_file_handle is nullptr
    [[nodiscard]] static skl_status setup_file_handle_sink(void* f_file_handle) noexcept;

    //! Set the current sink
    //! \returns SKL_ERR_PARAMS if \p f_id is out of range
    [[nodiscard]] static skl_status set_current_sink(slogger_sink_id_t f_id) noexcept;
};
} // namespace skl
