//!
//! \file skl_signal.cpp
//!
//! \brief os signal handler
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <cstdio>
#include <csignal>

#include "skl_core"
#include "skl_signal"
#include "skl_atomic"
#include "skl_spin_lock"
#include "skl_vector_if"

namespace {
__sighandler_t                       g_original_SIGABRT;                      //!<
__sighandler_t                       g_original_SIGFPE;                       //!<
__sighandler_t                       g_original_SIGILL;                       //!<
__sighandler_t                       g_original_SIGSEGV;                      //!<
__sighandler_t                       g_original_SIGINT;                       //!<
__sighandler_t                       g_original_SIGTERM;                      //!<
std::relaxed_value<bool>             g_program_epilog_init{false};            //!<
std::relaxed_value<bool>             g_exit_handler_called{false};            //!<
std::relaxed_value<bool>             g_abornal_exit_handler_called{false};    //!<
std::relaxed_value<bool>             g_termination_req_handler_called{false}; //!<
skl::skl_vector<skl::TEpilogHandler> g_exit_handlers{};                       //!<
skl::skl_vector<skl::TEpilogHandler> g_abnormal_exit_handlers{};              //!<
skl::skl_vector<skl::TEpilogHandler> g_termination_req_handlers{};            //!< 
skl::spin_lock_t                     g_sig_handlers_lock{};                   //!<
} // namespace

namespace {
void on_program_exit() noexcept {
    for (auto& handler : g_exit_handlers) {
        handler(0);
    }

    (void)skl::skl_core_deinit_thread();
    (void)::puts("PROGRAM EXIT!");
}
void on_program_abnormal_exit(int f_signal) noexcept {
    for (auto& handler : g_abnormal_exit_handlers) {
        handler(f_signal);
    }

    (void)skl::skl_core_deinit_thread();
    (void)::puts("ABNORMAL PROGRAM TERMINATION!");
}
void on_program_termination_request(int f_signal) noexcept {
    for (auto& handler : g_termination_req_handlers) {
        handler(f_signal);
    }

    (void)skl::skl_core_deinit_thread();
    (void)::puts("PROGRAM TERMINATION REQUESTED!");
}
} // namespace

namespace {
void _call_original_signal_handler(int f_signal) noexcept {
    switch (f_signal) {
        case SIGABRT:
            {
                if (nullptr != g_original_SIGABRT) {
                    g_original_SIGABRT(f_signal);
                }
            }
            break;
        case SIGFPE:
            {
                if (nullptr != g_original_SIGFPE) {
                    g_original_SIGFPE(f_signal);
                }
            }
            break;
        case SIGILL:
            {
                if (nullptr != g_original_SIGILL) {
                    g_original_SIGILL(f_signal);
                }
            }
            break;
        case SIGSEGV:
            {
                if (nullptr != g_original_SIGSEGV) {
                    g_original_SIGSEGV(f_signal);
                }
            }
            break;
        case SIGINT:
            {
                if (nullptr != g_original_SIGINT) {
                    g_original_SIGINT(f_signal);
                }
            }
            break;
        case SIGTERM:
            {
                if (nullptr != g_original_SIGTERM) {
                    g_original_SIGTERM(f_signal);
                }
            }
            break;
        default:
            {
            }
            break;
    }

    (void)skl::skl_core_deinit_thread();
}

void _exit_handler() noexcept {
    if (false == g_exit_handler_called.exchange(true)) {
        g_sig_handlers_lock.lock();
        on_program_exit();
        g_sig_handlers_lock.unlock();
    }
}

void _abnormal_exit_handler(int f_signal) noexcept {
    if (false == g_abornal_exit_handler_called.exchange(true)) {
        g_sig_handlers_lock.lock();
        on_program_abnormal_exit(f_signal);
        g_sig_handlers_lock.unlock();
    }

    _call_original_signal_handler(f_signal);
}

void _termination_request_handler(int f_signal) noexcept {
    if (false == g_termination_req_handler_called.exchange(true)) {
        g_sig_handlers_lock.lock();
        on_program_termination_request(f_signal);
        g_sig_handlers_lock.unlock();
    }

    _call_original_signal_handler(f_signal);
}
} // namespace

namespace skl {
skl_status init_program_epilog() noexcept {
    if (g_program_epilog_init.exchange(true)) {
        return SKL_OK_REPEAT;
    }

    //Register normal exit handler
    if (0 != std::atexit(&_exit_handler)) {
        return SKL_ERR_FAIL;
    }

    //Register abnormal exit handlers
    g_original_SIGABRT = ::signal(SIGABRT, &_abnormal_exit_handler); //Abort signal
    g_original_SIGFPE  = ::signal(SIGFPE, &_abnormal_exit_handler);  //Floating-point exception
    g_original_SIGILL  = ::signal(SIGILL, &_abnormal_exit_handler);  //Illegal instruction
    g_original_SIGSEGV = ::signal(SIGSEGV, &_abnormal_exit_handler); //Invalid memory reference

    //Register termination request handlers
    g_original_SIGINT  = ::signal(SIGINT, &_termination_request_handler);  //Interrupt signal (CTRL + C)
    g_original_SIGTERM = ::signal(SIGTERM, &_termination_request_handler); //Termination request

    //Reserve handlers space
    g_exit_handlers.upgrade().reserve(32U);
    g_abnormal_exit_handlers.upgrade().reserve(32U);
    g_termination_req_handlers.upgrade().reserve(32U);

    return SKL_SUCCESS;
}
skl_status register_epilog_handler(TEpilogHandler&& f_handler) noexcept {
    if (false == g_program_epilog_init) {
        return SKL_ERR_INIT;
    }

    g_sig_handlers_lock.lock();
    g_exit_handlers.upgrade().emplace_back(std::move(f_handler));
    g_sig_handlers_lock.unlock();
    return SKL_SUCCESS;
}
skl_status register_epilog_abnormal_handler(TEpilogHandler&& f_handler) noexcept {
    if (false == g_program_epilog_init) {
        return SKL_ERR_INIT;
    }

    g_sig_handlers_lock.lock();
    g_abnormal_exit_handlers.upgrade().emplace_back(std::move(f_handler));
    g_sig_handlers_lock.unlock();
    return SKL_SUCCESS;
}
skl_status register_epilog_termination_handler(TEpilogHandler&& f_handler) noexcept {
    if (false == g_program_epilog_init) {
        return SKL_ERR_INIT;
    }

    g_sig_handlers_lock.lock();
    g_termination_req_handlers.upgrade().emplace_back(std::move(f_handler));
    g_sig_handlers_lock.unlock();
    return SKL_SUCCESS;
}
} // namespace skl
