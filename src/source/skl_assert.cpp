//!
//! \file skl_assert
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include "skl_assert"

namespace skl {
[[noreturn]] void core_dump() noexcept;
}

/* Abort execution and generate a core-dump.  */
extern "C" void abort(void) noexcept __attribute__((__noreturn__));
extern "C" int  snprintf(char* __restrict __s, unsigned long __maxlen, const char* __restrict __format, ...) noexcept __attribute__((__format__(__printf__, 3, 4)));
extern "C" int  puts(const char* __s);
extern "C" int  open(const char* __file, int __oflag, ...) noexcept;
extern "C" long read(int __fd, void* __buf, unsigned long __nbytes) noexcept;
extern "C" int  close(int __fd) noexcept;

namespace {
thread_local char g_assert_message_buffer[4098U];

[[gnu::cold]] bool is_debugger_attached() noexcept {
    int fd = open("/proc/self/status", 0 /*O_RDONLY*/);
    if (fd < 0) {
        return false;
    }
    char buf[512];
    long n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';
    // Find "TracerPid:\t" and check if the value is non-zero
    for (long i = 0; i < n - 10; ++i) {
        if (buf[i] == 'T' && buf[i+1] == 'r' && buf[i+2] == 'a' && buf[i+3] == 'c' &&
            buf[i+4] == 'e' && buf[i+5] == 'r' && buf[i+6] == 'P' && buf[i+7] == 'i' &&
            buf[i+8] == 'd' && buf[i+9] == ':') {
            // Skip colon and whitespace
            long j = i + 10;
            while (j < n && (buf[j] == '\t' || buf[j] == ' ')) { ++j; }
            return (j < n && buf[j] != '0');
        }
    }
    return false;
}
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
    if (is_debugger_attached()) {
        __builtin_debugtrap();
    }
    skl::core_dump();
}
} // namespace skl_assert
