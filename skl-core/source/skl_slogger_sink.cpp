//!
//! \file skl_slogger_sink
//!
//! \brief serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <bits/types/FILE.h>

#include "skl_stream"
#include "skl_tls"
#include "skl_traits/size"
#include "skl_logger/skl_slogger_sink.hpp"
#include "skl_logger/skl_slogger_bend.hpp"

/*
*! @TODO: Implement all sinks | set default sink in skl_core_init_thread__slog()
*! CURRENT: direct format and output to stdout is the current/temp impl
*/

extern "C" FILE* stdout;
extern int       puts(const char* __s);

namespace {
struct SLoggerSinkThreadData {
    SLoggerSinkThreadData() noexcept {
        m_buffer[skl::array_size(m_buffer) - 1U] = 0;
    }

    [[nodiscard]] skl::skl_buffer_view& buffer() noexcept {
        return m_view;
    }

private:
    byte                 m_buffer[skl::CSerializedLoggerThreadBufferSize + 1U];
    skl::skl_buffer_view m_view{skl::array_size(m_buffer) - 1U, m_buffer};
};

struct SLoggerSinkManagement {
    SLoggerSinkManagement() noexcept {
        //Default to stdout file handle sink
        const auto setup_result = skl::SLoggerSink::setup_handle_sink(stdout);
        SKL_ASSERT_PERMANENT(setup_result.is_success());
        SKL_ASSERT_PERMANENT(0 == setup_result.value());
        const auto set_result = skl::SLoggerSink::set_current_sink(setup_result.value());
        SKL_ASSERT_PERMANENT(set_result.is_success());
    }

} g_sink{};
} // namespace

SKL_MAKE_TLS_SINGLETON(SLoggerSinkThreadData, g_sink_tls);

namespace skl {
skl_status SLoggerSink::init_thread() noexcept {
    return g_sink_tls::tls_create();
}
void SLoggerSink::deinit_thread() noexcept {
    g_sink_tls::tls_destroy();
}
skl_result<SLoggerSink::SinkId> SLoggerSink::setup_custom_sink(ISloggerSink* f_sink) noexcept {
    return skl_fail{};
}

skl_result<SLoggerSink::SinkId> SLoggerSink::setup_network_sink(const SLoggerNetworkSinkConfig& f_config) noexcept {
    return skl_fail{};
}

skl_result<SLoggerSink::SinkId> SLoggerSink::setup_file_sink(const SLoggerFileSinkConfig& f_config) noexcept {
    return skl_fail{};
}

skl_result<SLoggerSink::SinkId> SLoggerSink::setup_handle_sink(void* f_file_handle) noexcept {
    SKL_ASSERT_PERMANENT(f_file_handle == stdout);
    return 0;
}

skl_stream& SLoggerSink::begin_log() noexcept {
    auto& buffer = skl_stream::make(g_sink_tls::tls_guarded().buffer());
    buffer.reset();
    return buffer;
}

skl_status SLoggerSink::set_current_sink(SLoggerSink::SinkId f_id) noexcept {
    return SKL_SUCCESS;
}

void SLoggerSink::sink_log() noexcept {
    auto& buffer = skl_stream::make(g_sink_tls::tls_guarded().buffer());
    buffer.reset();
    auto final_str = SKLSerializedLoggerBackend::process(buffer);
    puts(final_str.data());
}
} // namespace skl
