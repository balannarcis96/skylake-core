//!
//! \file skl_slogger_sink_handle
//!
//! \brief serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>

#include <skl_atomic>
#include <bits/types/FILE.h>

#include "skl_stream"
#include "skl_logger/skl_slogger_sink.hpp"
#include "skl_logger/skl_slogger_bend.hpp"

/*
*! @TODO: Implement all sinks | set default sink in skl_core_init_thread__slog()
*! CURRENT: direct format and output to stdout is the current/temp impl
*/

extern "C" FILE* stdout;

namespace skl {
void skl_core_init_thread__slog_bend() noexcept;
void skl_core_deinit_thread__slog_bend() noexcept;

struct LoggerFileHandleSink final
    : public SLoggerSink {

    LoggerFileHandleSink() noexcept
        : SLoggerSink(false, CSLoggerFileHandleSinkId) { }

    void thread_init() noexcept override {
        //We need to initialize the backend processor on this thread
        skl_core_init_thread__slog_bend();
    }
    void thread_deinit() noexcept override {
        //We need to deinitialize the backend processor on this thread
        skl_core_deinit_thread__slog_bend();
    }

    void begin_log(skl_stream& f_log_stream) noexcept override { }

    void end_and_sink_log(skl_stream& f_log_stream) noexcept override {
        //@TODO: if we need to write sink specific data do it here

        //We are given the stream right after the log serilization is done, reset pos to 0
        f_log_stream.reset();

        //Process the log and puts(...)
        const auto& result = SKLSerializedLoggerBackend::process(f_log_stream);
        auto*       handle = reinterpret_cast<FILE*>(m_file_handle.load_acquire());
        (void)fputs(result.data(), handle);
        (void)fputs("\n", handle);
    }

    void set_file_handle(void* f_file_handle) noexcept {
        m_file_handle.store_release(f_file_handle);
    }

private:
    std::relaxed_value<void*> m_file_handle{stdout};
};
} // namespace skl

namespace {
SKL_CACHE_ALIGNED skl::LoggerFileHandleSink g_skl_file_handle_log_sink;
}

namespace skl {
void slogger_register_sink(SLoggerSink* f_sink) noexcept;

skl_status SLoggerSinkManager::setup_file_handle_sink(void* f_file_handle) noexcept {
    if (nullptr == f_file_handle) {
        return SKL_ERR_PARAMS;
    }

    g_skl_file_handle_log_sink.set_file_handle(f_file_handle);
    slogger_register_sink(&g_skl_file_handle_log_sink);

    return SKL_SUCCESS;
}
} // namespace skl
