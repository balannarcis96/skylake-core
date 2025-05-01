//!
//! \file skl_report
//!
//! \brief stats report utils
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <tune_skl_core_public.h>

#include "skl_spin_lock"
#include "skl_buffer_view"
#include "skl_tls"
#include "skl_thread_id"
#include "skl_fixed_vector_if"
#include "skl_traits/size"
#include "skl_report"
#include "skl_report_read"

struct SklReportQueueThreadData;

namespace {
//! Reports buffers lock
skl::spin_lock_t g_report_buffers_lock{};

//! All possible report buffer (1k threads max)
skl::skl_fixed_vector<SklReportQueueThreadData*, 1024ULL> g_report_buffers{};

//! Current read report buffer index
u64 g_report_current_buffer_index{0ULL};
} // namespace

struct SklReportQueueThreadData {
    skl::spin_lock_t     lock{};
    skl::skl_buffer_view view{skl::array_size(buffer), buffer};
    u32                  thread_id{0U};
    byte                 buffer[skl::CSklReportingThreadBufferSize];
};

SKL_MAKE_TLS_SINGLETON_CORE(SklReportQueueThreadData, g_skl_reporting, SKL_CACHE_LINE_SIZE);

namespace {
SKL_NOINLINE void skl_report_init_thread() noexcept {
    //Create the thread local buffer
    SKL_ASSERT_PERMANENT(g_skl_reporting::tls_create().is_success());

    //Set the thread id
    g_skl_reporting::tls_checked().thread_id = skl::current_thread_id();

    //Add it to the global list of buffers (make it available for reading)
    {
        skl::lock_guard_t guard{g_report_buffers_lock};
        SKL_ASSERT_PERMANENT(g_report_buffers.upgrade().push_back(&g_skl_reporting::tls_checked()));
    }
}
} // namespace

//Writing
namespace skl {
skl_stream& skl_report_begin() noexcept {
    if (false == g_skl_reporting::tls_init_status()) [[unlikely]] {
        skl_report_init_thread();
    }

    auto& tls = g_skl_reporting::tls_checked();

    //Lock the report buffer here
    tls.lock.lock();

    skl_stream& stream = skl_stream::make(tls.view);

    stream.reset();

    return stream;
}

void skl_report_submit() noexcept {
    auto& tls = g_skl_reporting::tls_checked();

    //Unlock the report buffer here
    tls.lock.unlock();
}
} // namespace skl

//Reading
namespace skl {
u64 skl_report_read_begin() noexcept {
    //Lock the global buffers list here
    g_report_buffers_lock.lock();

    const auto count = g_report_buffers.size();
    if (0ULL == count) {
        //Unlock if empty
        g_report_buffers_lock.unlock();
    } else {
        g_report_current_buffer_index = 0ULL;
    }

    return count;
}

skl_stream& skl_report_read_current_begin() noexcept {
    auto* buffer = g_report_buffers[g_report_current_buffer_index];
    SKL_ASSERT(nullptr != buffer);

    //Lock the report buffer here
    buffer->lock.lock();

    auto& stream = skl_stream::make(buffer->view);
    return stream;
}

void skl_report_read_current_end() noexcept {
    auto* buffer = g_report_buffers[g_report_current_buffer_index];
    SKL_ASSERT(nullptr != buffer);

    //Unlock the report buffer here
    buffer->lock.unlock();

    //Advance to the next buffer
    ++g_report_current_buffer_index;
}

void skl_report_read_end() noexcept {
    g_report_current_buffer_index = 0ULL;

    //Unlock the global buffers list here
    g_report_buffers_lock.unlock();
}
} // namespace skl
