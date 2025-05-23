#include <skl_log>
#include <skl_string_view>

#include <gtest/gtest.h>

#define SKL_LOG_TAG "[GTest] -- "

TEST(SkylakeLogger, General) {
    puts("no color!");
    STRACE;
    puts("no color!");
    SINFO_LOCAL_T("<Test> {}", 23);
    puts("no color!");
    SDEBUG_LOCAL_T("<Test> {}", 23);
    puts("no color!");
    SWARNING_LOCAL_T("<Test> {}", 23);
    puts("no color!");
    SERROR_LOCAL_T("<Test> {}", 23);
    puts("no color!");
    SFATAL_LOCAL_T("<Test> {}", this->test_info_->test_case_name());
    puts("no color!");
    SFATAL_LOCAL_T("<Test> {}", skl::skl_string_view::from_cstr(this->test_info_->test_case_name()));
    puts("no color!");
    SINFO_SPECIFIC(CSLoggerLocalSink, "<Test> {}", 23);
    puts("no color!");
    SDEBUG_SPECIFIC(CSLoggerLocalSink, "<Test> {}", 23);
    puts("no color!");
    SWARNING_SPECIFIC(CSLoggerLocalSink, "<Test> {}", 23);
    puts("no color!");
    SERROR_SPECIFIC(CSLoggerLocalSink, "<Test> {}", 23);
    puts("no color!");
    SFATAL_SPECIFIC(CSLoggerLocalSink, "<Test> {}", this->test_info_->test_case_name());
    puts("no color!");
    SFATAL_SPECIFIC(CSLoggerLocalSink, "<Test> {}", skl::skl_string_view::from_cstr(this->test_info_->test_case_name()));
    puts("no color!");
}

#undef SKL_LOG_TAG
