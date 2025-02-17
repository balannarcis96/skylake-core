#include <gtest/gtest.h>

#include <skl_log>
#include <skl_core>

class SKLCoreLogTest : public testing::Test {
public:
    static void SetUpTestSuite() {
        ASSERT_EQ(SKL_SUCCESS, skl::skl_core_init());
    }
    static void TearDownTestSuite() {
        ASSERT_EQ(SKL_SUCCESS, skl::skl_core_deinit());
    }
};

void test_fn_fn() noexcept {
    STRACE;
    SWARNING("Log: {} {} ", u64(0xFFFFFFFFFFFFFFFFU), "asdasd");
}

TEST_F(SKLCoreLogTest, log_basics) {
    STRACE;

    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    test_fn_fn();
    const char* str      = "Asdasdasd";
    char        buffer[] = "12412412";
    SERROR("Log: {} {} {} {} {}", 15, "asdasd", 15.0f, str, buffer);
}
