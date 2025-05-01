//!
//! \file skl_sleep
//!
//! \brief sleep utils
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <chrono>
#include <ctime>

#include <unistd.h>

#include "skl_sleep"

namespace skl {
void skl_sleep(u32 f_ms) noexcept {
    usleep(f_ms * 1000);
}
void skl_usleep(u32 f_us) noexcept {
    usleep(f_us);
}
void skl_precise_sleep(double f_seconds_to_sleep) noexcept {
    if (f_seconds_to_sleep <= 0.0) {
        return;
    }

    // 1 ms threshold
    constexpr double SPIN_THRESHOLD = 0.001;

    // If below threshold, spin; otherwise use nanosleep
    if (f_seconds_to_sleep < SPIN_THRESHOLD) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();

            std::chrono::duration<double> elapsed = now - start;
            if (elapsed.count() >= f_seconds_to_sleep) {
                break;
            }
        }
    } else [[likely]] {
        struct timespec ts;
        ts.tv_sec  = static_cast<time_t>(f_seconds_to_sleep);
        ts.tv_nsec = static_cast<long>((f_seconds_to_sleep - double(ts.tv_sec)) * 1e9);
        nanosleep(&ts, nullptr);
    }
}
} // namespace skl
