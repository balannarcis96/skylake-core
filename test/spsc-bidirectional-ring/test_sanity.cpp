#include <skl_spsc_bidirectional_ring>

#include <gtest/gtest.h>

namespace {
struct my_object_t {
    u64 numbers[64U];
};
} // namespace

TEST(SkylakeSPSCBidirectionalRing, Sanity) {
    spsc_bidirectional_ring_t<my_object_t, 32U> queue{};

    my_object_t* temp[queue.Size + 1U];
    ASSERT_FALSE(queue.allocate_bulk(temp, std::size(temp)));
}
