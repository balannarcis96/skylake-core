#include <gtest/gtest.h>
#include <fmt/format.h>

#include <skl_core>
#include <skl_thread>
#include <skl_thread_id>
#include <skl_sleep>
#include <skl_log>
#include <skl_epoch>
#include <skl_signal>

class SKLThreadTest : public testing::Test {
public:
    static void SetUpTestSuite() {
        ASSERT_EQ(SKL_SUCCESS, skl::skl_core_init());
    }
    static void TearDownTestSuite() {
        ASSERT_EQ(SKL_SUCCESS, skl::skl_core_deinit());
    }
};

TEST(SKLCoreThreadTest, thread_id) {
    ASSERT_DEATH((void)skl::current_thread_id_unsafe(), ".*");
    ASSERT_TRUE(skl::init_current_thread_id());
    ASSERT_TRUE(0 != skl::current_thread_id_unsafe());

    auto id = skl::current_thread_id_unsafe();
    for (auto i = 0; i < 32U; ++i) {
        const auto id2 = skl::current_thread_id_unsafe();
        ASSERT_EQ(id, id2);
    }

    fmt::println("tid: {}", id);
}

TEST_F(SKLThreadTest, thread) {
    ASSERT_TRUE(0 != skl::current_thread_id_unsafe());

    skl::SKLThread thread{};

    thread.set_handler([]() {
        SINFO("From test!");
        skl::skl_sleep(10U);
        return 0;
    });

    ASSERT_TRUE(thread.create().is_success());
    ASSERT_TRUE(thread.joinable());
    ASSERT_TRUE(thread.join().is_success());

    ASSERT_TRUE(skl::register_epilog_handler([](int) noexcept {
        puts("PAST CORE LIFETIME!");
    }).is_success());
}
