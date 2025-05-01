//!
//! \file skl_epoch
//!
//! \license Licensed under the MIT License. See LICENSE for details.
//!
#include <ctime>

#include "skl_assert"
#include "skl_epoch"

namespace skl {
[[nodiscard]] epoch_time_point_t get_current_epoch_time() noexcept {
    timespec   tsc{};
    const auto result = clock_gettime(CLOCK_REALTIME_COARSE, &tsc);
    SKL_ASSERT(-1 != result);
    (void)result;
    return static_cast<u64>((tsc.tv_sec * 1000) + (tsc.tv_nsec / 1000000));
}
} // namespace skl
