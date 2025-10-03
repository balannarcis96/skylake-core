//!
//! \file skl_slogger_fend
//!
//! \brief serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_epoch"
#include "skl_tls"
#include "skl_atomic"
#include "skl_logger/skl_slogger_fend.hpp"
#include "skl_logger/skl_slogger_sink.hpp"

static_assert(skl::CSerializedLoggerThreadBufferSize >= 4096U, "SKL::CSerializedLoggerThreadBufferSize must be at least 4096!");

extern int printf(const char* __restrict __format, ...);

namespace {
[[nodiscard]] inline u16 slogger_get_thread_uid() noexcept {
    static thread_local u16 thread_local_id = [] {
        static std::relaxed_value<u16> global_id{0U};
        return global_id.increment();
    }();
    return thread_local_id;
}
} // namespace

struct SLoggerThreadFrontEnd {
    SLoggerThreadFrontEnd() noexcept
        : thread_id(slogger_get_thread_uid())
        , start_timestamp(skl::get_current_epoch_time()) {
        //(void)printf("SKL SLOGGER -- Thread %d started at %llu\n", i32(thread_id), static_cast<unsigned long long>(start_timestamp));
    }

    const u16 thread_id       = 0U;
    const u64 start_timestamp = 0U;
};
SKL_MAKE_TLS_SINGLETON(SLoggerThreadFrontEnd, SLoggerFendTLS);

namespace skl {
skl_stream&       slogger_sink_begin_log() noexcept;
skl_stream&       slogger_sink_begin_log(slogger_sink_id_t f_specific_sink_id) noexcept;
void              slogger_sink_log() noexcept;
void              slogger_sink_log(slogger_sink_id_t f_specific_sink_id) noexcept;
SKL_NOINLINE void skl_core_init_logger_on_thread() noexcept;

skl_stream& skl_begin_log() noexcept {
    if (false == SLoggerFendTLS::tls_init_status()) [[unlikely]] {
        skl_core_init_logger_on_thread();
    }

    auto&      tls          = SLoggerFendTLS::tls_checked();
    const auto now          = get_current_epoch_time();
    const auto relative_now = u32(now - tls.start_timestamp);

    auto& stream = slogger_sink_begin_log();

    //1. Write timestamp
    stream.write<u32>(relative_now);

    //2. Write uid
    stream.write<u16>(tls.thread_id);

    return stream;
}
skl_stream& skl_begin_log(slogger_sink_id_t f_specific_sink_id) noexcept {
    if (false == SLoggerFendTLS::tls_init_status()) [[unlikely]] {
        skl_core_init_logger_on_thread();
    }

    auto&      tls          = SLoggerFendTLS::tls_checked();
    const auto now          = get_current_epoch_time();
    const auto relative_now = u32(now - tls.start_timestamp);

    auto& stream = slogger_sink_begin_log(f_specific_sink_id);

    //1. Write timestamp
    stream.write<u32>(relative_now);

    //2. Write uid
    stream.write<u16>(tls.thread_id);

    return stream;
}
void skl_commit_log() noexcept {
    slogger_sink_log();
}
void skl_commit_log(slogger_sink_id_t f_specific_sink_id) noexcept {
    slogger_sink_log(f_specific_sink_id);
}

SKL_NOINLINE void skl_core_init_logger_on_thread() noexcept {
    SKL_ASSERT_PERMANENT(SLoggerFendTLS::tls_create().is_success());
    SKL_ASSERT_PERMANENT(SLoggerSinkManager::init_thread().is_success());
}
} // namespace skl

namespace skl {
void skl_core_deinit_thread__slog() noexcept {
    SLoggerSinkManager::deinit_thread();
    SLoggerFendTLS::tls_destroy();
}
} // namespace skl
