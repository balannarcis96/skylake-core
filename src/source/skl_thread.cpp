//!
//! \file skl_thread
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>

#include <pthread.h>

#include "skl_thread"
#include "skl_log"
#include "skl_core"

namespace {
[[nodiscard]] bool set_thread_affinity_impl(skl::ThreadHandle f_handle, skl::pair<i16, i16> f_cpu_index_range) noexcept {
    if (f_handle == skl::CInvalidThreadHandle) {
        SWARNING_LOCAL("SetThreadAffinityImpl({}, {}, {}) Invalid thread handle!", f_handle, f_cpu_index_range.first, f_cpu_index_range.second);
        return false;
    }

    if (f_cpu_index_range.first > f_cpu_index_range.second) {
        SWARNING_LOCAL("SetThreadAffinityImpl({}, {}, {}) Invalid cpu index range!", f_handle, f_cpu_index_range.first, f_cpu_index_range.second);
        return false;
    }

    cpu_set_t set;
    CPU_ZERO(&set);

    if (f_cpu_index_range.first == f_cpu_index_range.second) {
        CPU_SET(f_cpu_index_range.first, &set);
    } else {
        for (i32 cpu_index = f_cpu_index_range.first; cpu_index <= f_cpu_index_range.second; ++cpu_index) {
            CPU_SET(cpu_index, &set);
        }
    }

    const auto result = ::pthread_setaffinity_np(f_handle, sizeof(cpu_set_t), &set);
    if (0 != result) {
        SWARNING_LOCAL("SetThreadAffinityImpl({}, {}, {}) Failed!", f_handle, f_cpu_index_range.first, f_cpu_index_range.second);
        return false;
    }

    return true;
}
} // namespace

namespace skl {
static_assert(__is_same(pthread_t, ThreadHandle));
static_assert(sizeof(pthread_t) == sizeof(ThreadHandle));

void* thread_run_proxy(void* f_arg) noexcept {
    SKL_ASSERT(nullptr != f_arg);
    SKLThread& self{*reinterpret_cast<SKLThread*>(f_arg)};

    //Wait for the start signal
    if (nullptr != self.m_start_sync) {
        self.m_start_sync->wait();
    }

    if (skl_core_init_thread().is_failure()) {
        puts("Thread:: Failed to ini the skl core lib!");
        (void)self.m_result_value.exchange(SKL_ERR_INIT);
        return nullptr;
    }

    //Check if we should continue
    if (false == self.m_bRun.load_acquire()) {
        SFATAL_LOCAL("Thread:: Failed to start! Stopped early!");
        (void)self.m_result_value.exchange(SKL_ERR_ABORT);
        return nullptr;
    }

    //Yield back to the scheduler so the affinity settings set in
    if (0 != sched_yield()) {
        SFATAL_LOCAL("Thread:: Failed to start! Call to sched_yield failed!");
        (void)self.m_result_value.exchange(SKL_ERR_STATE);
        return nullptr;
    }

#if 0
    SDEBUG_LOCAL("Thread[{}] started successfully!", self.name().data());
#endif

#if !SKL_NO_EXCEPTIONS
    try {
#endif
        //Call the execution handler
        const auto result = self.call_handler();
        (void)self.m_result_value.exchange(result);
#if !SKL_NO_EXCEPTIONS
    } catch (const std::exception& ex) {
        printf("Thread[%s] Failed with exception! [%s]\n", self.name().data(), ex.what());
    }
#endif

#if 0
    SDEBUG_LOCAL("Thread[{}] ended!", self.name().data());
#endif

    if (skl_core_deinit_thread().is_failure()) {
        puts("Thread:: Failed to deini the skl core lib!");
    }

    return nullptr;
}

SKLThread::~SKLThread() noexcept {
    (void)join();
}

skl_status SKLThread::create(SKLThreadAffinity f_cpu_index_range) noexcept {
    if (m_handle != CInvalidThreadHandle) {
        SWARNING_LOCAL("Attempting to create and already created thread!");
        return SKL_ERR_REPEAT;
    }

    pthread_t new_handle{};

    //Reset the latch
    m_start_sync = std::make_unique<std::latch>(1U);

    const i32 create_result{::pthread_create(&new_handle, nullptr, &thread_run_proxy, this)};
    if (0 != create_result) {
        SERROR_LOCAL("Failed to create thread!");
        return SKL_ERR_ALLOC;
    }

    if ((f_cpu_index_range.first >= 0) && (f_cpu_index_range.second >= 0)) {
        //Set affinity
        if (false == set_thread_affinity_impl(new_handle, f_cpu_index_range)) {
            SERROR_LOCAL("SKLThread::Create() Failed to set new thread affinity Range[{} {}]!", f_cpu_index_range.first, f_cpu_index_range.second);

            //Set start failed. Stop early.
            (void)m_bRun.exchange(false);

            //Increment the latch so the thread can continue
            m_start_sync->count_down();

            //Join the thread
            (void)::pthread_join(new_handle, nullptr);

            return SKL_ERR_INIT;
        }
    }

    //Set joinable
    (void)m_bIsJoinable.exchange(true);

    //Set start success
    (void)m_bRun.exchange(true);

    //Increment the latch so the thread can continue
    m_start_sync->count_down();

    //Save the handle
    (void)m_handle.exchange(new_handle);

    return SKL_SUCCESS;
}

skl_status SKLThread::join() noexcept {
    if (false == m_bIsJoinable.load_acquire()) {
        return SKL_ERR_OP_ORDER;
    }

    const auto old_handle = m_handle.exchange(CInvalidThreadHandle);
    if (old_handle == CInvalidThreadHandle) {
        return SKL_ERR_STATE;
    }

    const i32 result = ::pthread_join(old_handle, nullptr);
    if (0 != result) {
        SERROR_LOCAL("SKLThread[{}] pthread_join(...) call failed with {}!", name().data(), result);
        [[unlikely]] return SKL_ERR_FAIL;
    }

    return SKL_SUCCESS;
}

skl_status SKLThread::detach() noexcept {
    const auto old_handle = m_handle.exchange(CInvalidThreadHandle);
    if (CInvalidThreadHandle != old_handle) {
        if (m_bIsJoinable.exchange(false)) {
            const i32 result = ::pthread_detach(old_handle);
            if (0 != result) {
                SERROR_LOCAL("SKLThread[{}] pthread_detach(...) call failed with {}!", name().data(), result);
                [[unlikely]] return SKL_ERR_FAIL;
            }
        }
    }

    return SKL_SUCCESS;
}
} // namespace skl
