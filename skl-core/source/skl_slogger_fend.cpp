//!
//! \file skl_slogger_fend
//!
//! \brief serialized logger front-end
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_epoch"
#include "skl_tls"
#include "skl_logger/skl_slogger_fend.hpp"
#include "skl_logger/skl_slogger_sink.hpp"

static_assert(skl::CSerializedLoggerThreadBufferSize >= 4096U, "SKL::CSerializedLoggerThreadBufferSize must be at least 4096!");

extern int printf(const char* __restrict __format, ...);

namespace {
[[nodiscard]] inline u16 slogger_get_thread_uid() noexcept {
    static thread_local u16 thread_local_id = [] {
        static u16 globalId = 0U;
        return globalId++;
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

namespace skl::skl_log_internal {
skl_stream& skl_begin_log() noexcept {
    auto& tls = SLoggerFendTLS::tls_guarded();

    const auto now          = get_current_epoch_time();
    const auto relative_now = u32(now - tls.start_timestamp);
    auto&      stream       = SLoggerSink::begin_log();

    //1. Write timestamp
    stream.write<u32>(relative_now);

    //2. Write uid
    stream.write<u16>(tls.thread_id);

    return stream;
}

void skl_commit_log() noexcept {
    SLoggerSink::sink_log();
}
} // namespace skl::skl_log_internal

namespace skl {
skl_status skl_core_init_thread__slog() noexcept {
    if (SLoggerSink::init_thread().is_failure()) {
        return SKL_ERR_TLS_INIT;
    }

    if (SLoggerFendTLS::tls_create().is_failure()) {
        return SKL_ERR_TLS_INIT;
    }

    //@TODO set default sink here

    return SKL_SUCCESS;
}
skl_status skl_core_deinit_thread__slog() noexcept {
    SLoggerFendTLS::tls_destroy();
    SLoggerSink::deinit_thread();
    return SKL_SUCCESS;
}
} // namespace skl
