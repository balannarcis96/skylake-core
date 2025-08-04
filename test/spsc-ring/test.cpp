#include <skl_spsc_ring>

#include <skl_thread>
#include <skl_core>
#include <skl_signal>
#include <skl_epoch>
#include <skl_sleep>

#include <gtest/gtest.h>

namespace {
struct my_object_t {
    u64  numbers[64U];
    u64  order;
    bool is_free = true;

    void reset() noexcept {
        is_free = true;
    }
};

spsc_ring_t<my_object_t, 1024U> g_queue{};
SKL_CACHE_ALIGNED std::relaxed_value<bool> g_run{true};
} // namespace
