//!
//! \file skl_slogger_sink
//!
//! \brief serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>

#include <skl_atomic>
#include <bits/types/FILE.h>

#include "skl_stream"
#include "skl_tls"
#include "skl_traits/size"
#include "skl_logger/skl_slogger_sink.hpp"

extern "C" FILE* stdout;

namespace {
//! Current global sink type to use
SKL_CACHE_ALIGNED std::relaxed_value<skl::slogger_sink_id_t> g_skl_current_log_sink = 0;

//! All sinks
SKL_CACHE_ALIGNED std::relaxed_value<skl::SLoggerSink*> g_skl_logger_sinks[i32(skl::ESLoggerSinkType::MAX)];

//! Has performed default init
std::relaxed_value<bool> g_skl_current_log_init_default = false;

struct slogger_sink_tls {
    slogger_sink_tls() noexcept {
        m_buffer[skl::array_size(m_buffer) - 1U] = 0;
    }

    [[nodiscard]] skl::skl_buffer_view& buffer() noexcept {
        return m_view;
    }

    [[nodiscard]] bool has_sink(skl::slogger_sink_id_t f_id) const noexcept {
        return (m_sink != nullptr) && (m_sink->id() == f_id);
    }

    void set_current_sink(skl::SLoggerSink* f_sink) noexcept {
        m_sink = f_sink;
    }

    [[nodiscard]] skl::SLoggerSink& sink() noexcept {
        return *m_sink;
    }

    [[nodiscard]] skl::SLoggerSink* sink_safe() noexcept {
        return m_sink;
    }

private:
    skl::SLoggerSink*    m_sink{nullptr};
    byte                 m_buffer[skl::CSerializedLoggerThreadBufferSize + 1U];
    skl::skl_buffer_view m_view{skl::array_size(m_buffer) - 1U, m_buffer};
};
} // namespace

SKL_MAKE_TLS_SINGLETON(slogger_sink_tls, g_sink_tls);

namespace skl {
void slogger_register_sink(SLoggerSink* f_sink) noexcept {
    SKL_ASSERT_PERMANENT(nullptr != f_sink);
    g_skl_logger_sinks[f_sink->id()].store_release(f_sink);
}

skl_status SLoggerSinkManager::init_thread() noexcept {
    if (g_sink_tls::tls_create().is_failure()) {
        return SKL_ERR_TLS_INIT;
    }

    if (false == g_skl_current_log_init_default.exchange(true)) {
        //Default to stdout file handle sink
        SKL_ASSERT_PERMANENT(setup_file_handle_sink(stdout).is_success());
        SKL_ASSERT_PERMANENT(set_current_sink(CSLoggerFileHandleSinkId).is_success());
    }

    return SKL_SUCCESS;
}

void SLoggerSinkManager::deinit_thread() noexcept {
    g_sink_tls::tls_destroy();
}

skl_status SLoggerSinkManager::register_custom_sink(SLoggerSink* f_sink) noexcept {
    if (nullptr == f_sink) {
        return SKL_ERR_PARAMS;
    }

    SKL_ASSERT(f_sink->id() == CSLoggerCustomSink);

    g_skl_logger_sinks[CSLoggerCustomSink].store_release(f_sink);

    return SKL_SUCCESS;
}

skl_status SLoggerSinkManager::setup_network_sink(const slogger_net_sink_config_t& f_config) noexcept {
    (void)f_config;
    return SKL_ERR_NOT_IMPL;
}

skl_status SLoggerSinkManager::setup_file_sink(const slogger_file_sink_config_t& f_config) noexcept {
    (void)f_config;
    return SKL_ERR_NOT_IMPL;
}

skl_status SLoggerSinkManager::set_current_sink(slogger_sink_id_t f_id) noexcept {
    if ((f_id < CSLoggerNetSinkId) || (f_id > CSLoggerCustomSink)) {
        return SKL_ERR_PARAMS;
    }

    if (g_skl_current_log_sink == f_id) {
        return SKL_OK_REDUNDANT;
    }

    g_skl_current_log_sink.store_release(f_id);

    return SKL_SUCCESS;
}
} // namespace skl

namespace {
SKL_NOINLINE void update_slogger_thread_sink(skl::slogger_sink_id_t f_id) noexcept {
    SKL_ASSERT((f_id >= skl::CSLoggerNetSinkId) && (f_id <= skl::CSLoggerCustomSink));

    auto& tls = g_sink_tls::tls_checked();

    auto* old_sink = tls.sink_safe();
    auto* new_sink = g_skl_logger_sinks[f_id].load_acquire();
    if (nullptr == new_sink) {
        (void)puts("NO SINK FOUND FOR THE GIVEN SINK ID!");
        SKL_ASSERT_PERMANENT(false);
    }

    SKL_ASSERT(old_sink != new_sink);

    if (nullptr != old_sink) {
        old_sink->thread_deinit();
    }

    new_sink->thread_init();
    tls.set_current_sink(new_sink);
}
} // namespace

namespace skl {
skl_stream& slogger_sink_begin_log() noexcept {
    const auto current_sink = g_skl_current_log_sink.load_acquire();
    auto&      tls          = g_sink_tls::tls_checked();

    if (false == tls.has_sink(current_sink)) [[unlikely]] {
        update_slogger_thread_sink(current_sink);
    }

    //Reset the sink buffer
    auto& buffer = skl_stream::make(tls.buffer());
    buffer.reset();

    //Get current sink ptr
    auto& sink = tls.sink();

    //Have the sink write its part in the log buffer
    if (sink.has_begin()) {
        sink.begin_log(buffer);
    }

    return buffer;
}

skl_stream& slogger_sink_begin_log(slogger_sink_id_t f_specific_sink_id) noexcept {
    auto* sink = g_skl_logger_sinks[f_specific_sink_id].load_relaxed();
    SKL_ASSERT(nullptr != sink);

    auto& tls = g_sink_tls::tls_checked();

    //Reset the sink buffer
    auto& buffer = skl_stream::make(tls.buffer());
    buffer.reset();

    //Have the sink write its part in the log buffer
    if (sink->has_begin()) {
        sink->begin_log(buffer);
    }

    return buffer;
}

void slogger_sink_log() noexcept {
    auto& tls    = g_sink_tls::tls_checked();
    auto& buffer = skl_stream::make(tls.buffer());
    auto& sink   = tls.sink();
    sink.end_and_sink_log(buffer);
}

void slogger_sink_log(slogger_sink_id_t f_specific_sink_id) noexcept {
    auto& tls    = g_sink_tls::tls_checked();
    auto& buffer = skl_stream::make(tls.buffer());
    auto* sink   = g_skl_logger_sinks[f_specific_sink_id].load_relaxed();
    SKL_ASSERT(nullptr != sink);
    sink->end_and_sink_log(buffer);
}
} // namespace skl
