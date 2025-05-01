//!
//! \file skl_timer
//!
//! \brief timer unitlity
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <ctime>

#include "skl_timer"
#include "skl_assert"

namespace {
double current_monotonic_timestamp_seconds() noexcept {
    struct timespec tsc{};
    const auto      result = ::clock_gettime(CLOCK_MONOTONIC_RAW, &tsc);
    (void)result;
    SKL_ASSERT(-1 != result);
    return static_cast<double>(tsc.tv_sec) + (static_cast<double>(tsc.tv_nsec) / 1000000000.0);
}
} // namespace

namespace skl {
void Timer::reset() noexcept {
    m_start      = current_monotonic_timestamp_seconds();
    m_total_time = 0.0;
    m_elapsed    = 0.0;
}
double Timer::tick() noexcept {
    const auto now = current_monotonic_timestamp_seconds();

    m_elapsed     = now - m_start;
    m_start       = now;
    m_total_time += m_elapsed;

    return m_total_time;
}
} // namespace skl
