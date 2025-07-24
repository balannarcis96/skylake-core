//!
//! \file skl_assert
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_assert"

/* Abort execution and generate a core-dump.  */
extern "C" void abort(void) noexcept __attribute__((__noreturn__));
extern "C" int  snprintf(char* __restrict __s, unsigned long __maxlen, const char* __restrict __format, ...) noexcept __attribute__((__format__(__printf__, 3, 4)));
extern "C" int  puts(const char* __s);

namespace {
thread_local char g_assert_message_buffer[4098U];
}

static_assert(__has_builtin(__builtin_debugtrap));

namespace skl_assert {
SKL_NOINLINE [[noreturn]] [[gnu::cold]] void handle_assert_failure(const char* f_file_name, int f_line_number, const char* f_expression) noexcept {
    (void)snprintf(g_assert_message_buffer,
                   sizeof(g_assert_message_buffer) / (sizeof(g_assert_message_buffer[0])),
                   "\u001b[31mAssert \"%s\" failed!\nAt:%s:%lu\n\u001b[37m",
                   f_expression,
                   f_file_name,
                   static_cast<u64>(f_line_number));
    puts(g_assert_message_buffer);
    __builtin_debugtrap();
    abort();
}
} // namespace skl_assert
