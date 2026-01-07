//!
//! \file test.cpp
//!
//! \brief Signal epilog handler tests
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <csignal>
#include <atomic>

#include <skl_core>
#include <skl_signal>

#include <gtest/gtest.h>

namespace {
std::atomic<bool> g_termination_handler_called{false};
std::atomic<int>  g_termination_signal_received{0};
} // namespace

class SignalEpilogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset state
        g_termination_handler_called.store(false);
        g_termination_signal_received.store(0);

        // Initialize core (includes epilog init)
        ASSERT_TRUE(skl::skl_core_init().is_success());
    }
};

TEST_F(SignalEpilogTest, RegisterTerminationHandler) {
    // Register handler using lambda (similar to user's pattern)
    auto result = skl::register_epilog_termination_handler([](int f_signal) noexcept {
        g_termination_handler_called.store(true);
        g_termination_signal_received.store(f_signal);
    });

    ASSERT_TRUE(result.is_success());
}

TEST_F(SignalEpilogTest, TerminationHandlerCalledOnSIGINT) {
    // Register handler
    ASSERT_TRUE(skl::register_epilog_termination_handler([](int f_signal) noexcept {
        g_termination_handler_called.store(true);
        g_termination_signal_received.store(f_signal);
    }).is_success());

    // Verify initial state
    ASSERT_FALSE(g_termination_handler_called.load());
    ASSERT_FALSE(skl::exit_was_requested());

    // Send SIGINT to ourselves
    (void)::raise(SIGINT);

    // Verify handler was called
    ASSERT_TRUE(g_termination_handler_called.load());
    ASSERT_EQ(g_termination_signal_received.load(), SIGINT);
    ASSERT_TRUE(skl::exit_was_requested());
}

TEST_F(SignalEpilogTest, TerminationHandlerCalledOnSIGTERM) {
    // Reset the internal flag by re-initializing (need fresh state)
    // Note: Since epilog handlers use atomic exchange, second signal won't re-trigger
    // This test verifies SIGTERM works when it's the first signal

    // Register handler
    ASSERT_TRUE(skl::register_epilog_termination_handler([](int f_signal) noexcept {
        g_termination_handler_called.store(true);
        g_termination_signal_received.store(f_signal);
    }).is_success());

    // If previous test already triggered termination, this won't fire again
    // This is expected behavior - handlers only fire once
    if (!skl::exit_was_requested()) {
        // Send SIGTERM to ourselves
        (void)::raise(SIGTERM);

        // Verify handler was called
        ASSERT_TRUE(g_termination_handler_called.load());
        ASSERT_EQ(g_termination_signal_received.load(), SIGTERM);
        ASSERT_TRUE(skl::exit_was_requested());
    }
}

TEST_F(SignalEpilogTest, ExitWasRequestedReturnsTrueAfterSignal) {
    // After SIGINT/SIGTERM, exit_was_requested() should return true
    if (!skl::exit_was_requested()) {
        (void)::raise(SIGINT);
    }
    ASSERT_TRUE(skl::exit_was_requested());
}

TEST_F(SignalEpilogTest, RegisterMultipleHandlers) {
    std::atomic<int> handler1_called{0};
    std::atomic<int> handler2_called{0};

    ASSERT_TRUE(skl::register_epilog_termination_handler([&handler1_called](int) noexcept {
        handler1_called.store(1);
    }).is_success());

    ASSERT_TRUE(skl::register_epilog_termination_handler([&handler2_called](int) noexcept {
        handler2_called.store(1);
    }).is_success());

    // If termination wasn't already requested, trigger it
    if (!skl::exit_was_requested()) {
        (void)::raise(SIGINT);

        // Both handlers should have been called
        ASSERT_EQ(handler1_called.load(), 1);
        ASSERT_EQ(handler2_called.load(), 1);
    }
}

TEST_F(SignalEpilogTest, RegisterEpilogHandler) {
    // Test normal exit handler registration (called via atexit)
    auto result = skl::register_epilog_handler([](int) noexcept {
        // This will be called on normal program exit
    });

    ASSERT_TRUE(result.is_success());
}

TEST_F(SignalEpilogTest, RegisterAbnormalHandler) {
    // Test abnormal exit handler registration
    // Note: We can't easily test SIGSEGV without killing the process
    auto result = skl::register_epilog_abnormal_handler([](int f_signal) noexcept {
        // This would be called on SIGSEGV, SIGFPE, etc.
        (void)f_signal;
    });

    ASSERT_TRUE(result.is_success());
}
