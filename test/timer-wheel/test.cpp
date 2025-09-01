#include <skl_rand>
#include <skl_sleep>
#include <skl_debug_trap>
#include <skl_timing/timer_wheel>

#include <gtest/gtest.h>

struct data_t {
    void reset() noexcept {
        value = 0u;
    }

    u32 value{0u};
};

using wheel_t = skl::TimerWheel<
    data_t, // User data type
    500u,   // Wheel time in ms
    10u,    // Granularity in ms
    3u,     // Tolerance in ms for tick time
    true>;

static_assert(wheel_t::CWheelTimeMs == 500u);
static_assert(wheel_t::CGranularityMs == 10u);
static_assert(wheel_t::CWheelSlotsCount == 50u);

using noop_tick_functor_t = decltype([](wheel_t::wheel_entry_t&) static noexcept { });

TEST(SkylakeTimerWheel, basic_construction) {
    {
        wheel_t wheel{};
        ASSERT_EQ(wheel.tick_count(), 0u);
        ASSERT_TRUE(wheel.can_tick());
    }

    {
        wheel_t wheel{};
        wheel.set_start_time();
        ASSERT_EQ(wheel.tick_count(), 0u);
        ASSERT_FALSE(wheel.can_tick());
    }
}

TEST(SkylakeTimerWheel, calculate_granularity_count) {
    ASSERT_EQ(wheel_t::calculate_granularity_count(0u), 0u);
    ASSERT_EQ(wheel_t::calculate_granularity_count(wheel_t::CGranularityMs - 1u), 0u);
    ASSERT_EQ(wheel_t::calculate_granularity_count(wheel_t::CGranularityMs), 1u);
    ASSERT_EQ(wheel_t::calculate_granularity_count(static_cast<u64>(wheel_t::CGranularityMs * 3u)), 3ull);
    ASSERT_EQ(wheel_t::calculate_granularity_count(wheel_t::CWheelTimeMs), wheel_t::CWheelSlotsCount - 1u);
    ASSERT_EQ(wheel_t::calculate_granularity_count(wheel_t::CWheelTimeMs + 1u), wheel_t::CWheelSlotsCount - 1u);
}

TEST(SkylakeTimerWheel, allocate_entry) {
    wheel_t wheel{};

    auto [handle, data_ptr] = wheel.allocate(9u, data_t{42u});
    ASSERT_NE(data_ptr, nullptr);
    ASSERT_EQ(data_ptr->value, 42u);
    ASSERT_EQ(handle.slot_index, 0u);
    ASSERT_EQ(handle.entry_index, 0u);
}

TEST(SkylakeTimerWheel, allocate_multiple_entries_same_slot) {
    wheel_t wheel = {};

    auto [handle1, data1] = wheel.allocate(1u, data_t{42u});
    auto [handle2, data2] = wheel.allocate(1u, data_t{84u});

    ASSERT_EQ(handle1.slot_index, handle2.slot_index);
    ASSERT_EQ(handle1.entry_index, 0u);
    ASSERT_EQ(handle2.entry_index, 1u);
    ASSERT_EQ(data1->value, 42u);
    ASSERT_EQ(data2->value, 84u);
}

TEST(SkylakeTimerWheel, bitset_count_tracking) {
    wheel_t                                wheel = {};
    std::vector<skl::timer_wheel_handle_t> handles;

    // Fill all slots
    const u32 CEntriesToAdd = 10000u;

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount; ++i) {
        for (u32 j = 0u; j < CEntriesToAdd; ++j) {
            auto result = wheel.allocate(u64(i) * wheel_t::CGranularityMs, data_t{j});
            handles.push_back(result.first);
        }
    }

    // Validate count via bitset
    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount; ++i) {
        SKL_ASSERT(wheel.get_set_bits_count(i) == CEntriesToAdd);
    }
}

TEST(SkylakeTimerWheel, validate_handle) {
    wheel_t wheel{};

    auto [handle, data_ptr] = wheel.allocate(0u, data_t{42u});
    ASSERT_TRUE(wheel.validate_handle(handle));

    skl::timer_wheel_handle_t invalid_handle{
        .slot_index  = wheel_t::CWheelSlotsCount, // Out of bounds
        .generation  = 0u,
        .entry_index = 0u};
    ASSERT_FALSE(wheel.validate_handle(invalid_handle));
}

TEST(SkylakeTimerWheel, advance_clears_slot) {
    wheel_t wheel           = {};
    auto [handle, data_ptr] = wheel.allocate(0u, data_t{42u});
    ASSERT_TRUE(wheel.validate_handle(handle));

    auto& slot_before = wheel.pending_slot();
    ASSERT_EQ(slot_before.size(), 1u);
    ASSERT_EQ(wheel.tick_count(), 0u);

    ASSERT_TRUE(wheel.template tick<noop_tick_functor_t>());

    auto& slot_after = wheel.pending_slot();
    ASSERT_NE(&slot_after, &slot_before);
    ASSERT_EQ(slot_after.size(), 0u);
    ASSERT_EQ(wheel.tick_count(), 1u);
    ASSERT_FALSE(wheel.validate_handle(handle));
}

TEST(SkylakeTimerWheel, wrap_around_slot_calculation) {
    wheel_t wheel           = {};
    auto [handle, data_ptr] = wheel.allocate(wheel_t::CWheelTimeMs, data_t{42u});
    ASSERT_EQ(handle.slot_index, wheel_t::CWheelSlotsCount - 1u);

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount - 1u; ++i) {
        skl::skl_busy_sleep(wheel_t::CGranularityMs);
        ASSERT_TRUE(wheel.template tick<noop_tick_functor_t>());
    }

    ASSERT_EQ(wheel.tick_count(), wheel_t::CWheelSlotsCount - 1u);

    {
        skl::skl_busy_sleep(wheel_t::CGranularityMs);
        auto& slot = wheel.pending_slot();
        ASSERT_EQ(slot.size(), 1u);
        ASSERT_EQ(slot.front().value, 42u);
    }
}

TEST(SkylakeTimerWheel, current_slot_access) {
    wheel_t wheel       = {};
    auto&   slot_before = wheel.pending_slot();
    ASSERT_EQ(slot_before.size(), 0u);

    (void)wheel.allocate(1u, data_t{42u});
    ASSERT_EQ(slot_before.size(), 1u);

    ASSERT_TRUE(wheel.template tick<noop_tick_functor_t>());

    auto& slot_after = wheel.pending_slot();
    ASSERT_EQ(slot_after.size(), 0u);
}

TEST(SkylakeTimerWheel, disable_entry_functionality) {
    wheel_t wheel           = {};
    auto [handle, data_ptr] = wheel.allocate(1u, data_t{42u});
    ASSERT_EQ(wheel.is_entry_enabled(handle), SKL_SUCCESS);

    wheel.disable_entry(handle);
    ASSERT_EQ(wheel.is_entry_enabled(handle), SKL_ERR_STATE);
}

TEST(SkylakeTimerWheel, generation_validation) {
    wheel_t wheel           = {};
    auto [handle, data_ptr] = wheel.allocate(1u, data_t{42u});
    ASSERT_TRUE(wheel.validate_handle_generation(handle));

    ASSERT_TRUE(wheel.template tick<noop_tick_functor_t>());

    ASSERT_FALSE(wheel.validate_handle_generation(handle));
}

TEST(SkylakeTimerWheel, dynamic_slots_no_overflow) {
    wheel_t wheel{};
    for (u32 i = 0u; i < 10000u; ++i) {
        auto result = wheel.allocate(1u, data_t{i});
        ASSERT_TRUE(result.first.entry_index == i);
    }
}

TEST(SkylakeTimerWheel, allocations_pending_slot_only) {
    wheel_t wheel{};

    std::vector<skl::timer_wheel_handle_t> handles;
    handles.reserve(1u << 17u);

    const auto start_time = skl::get_current_epoch_time();
    wheel.set_start_time();
    while ((skl::get_current_epoch_time() - start_time) <= 1500u) {
        handles.clear();

        constexpr auto CAllocCount = 50000u;
        for (u32 i = 0u; i < CAllocCount; ++i) {
            const auto set_bits_count_before = wheel.bitset(wheel.pending_slot_index()).count();
            auto       res                   = wheel.allocate(0u, data_t{i});
            SKL_ASSERT(res.first.slot_index == wheel.pending_slot_index());
            const auto set_bits_count_after = wheel.bitset(wheel.pending_slot_index()).count();
            SKL_ASSERT(set_bits_count_after == set_bits_count_before + 1u);
            handles.push_back(res.first);
        }

        ASSERT_EQ(CAllocCount, wheel.get_set_bits_count(wheel.pending_slot_index()));
        const auto pending_slot_index = wheel.pending_slot_index();

        u32 tick_count = 0u;
        while (false == wheel.tick(skl::get_current_epoch_time(), [&tick_count](wheel_t::wheel_entry_t&) noexcept { ++tick_count; })) { }
        ASSERT_EQ(tick_count, CAllocCount);
        ASSERT_EQ(wheel.get_set_bits_count(pending_slot_index), 0u);
        ASSERT_TRUE(pending_slot_index != wheel.pending_slot_index());

        skl::skl_busy_sleep(1000u / 144u);
    }
}

TEST(SkylakeTimerWheel, allocations_random_delay) {
    wheel_t wheel{};

    std::vector<skl::timer_wheel_handle_t> handles;
    handles.reserve(1u << 17u);
    constexpr auto CAllocCount           = 50000u;
    u32            wheel_ticked_items    = 0u;
    u32            total_allocated_items = 0u;
    u32            test_ticks_count      = 0u;

    const auto start_time = skl::get_current_epoch_time();
    wheel.set_start_time();
    while ((skl::get_current_epoch_time() - start_time) <= 1500u) {
        handles.clear();

        for (u32 i = 0u; i < CAllocCount; ++i) {
            const auto delay      = skl::get_thread_rand().next_range(0u, wheel_t::CWheelTimeMs * 2u);
            u32        slot_index = wheel.pending_slot_index() + wheel.calculate_granularity_count(delay);
            if (slot_index >= wheel_t::CWheelSlotsCount) {
                slot_index -= wheel_t::CWheelSlotsCount;
            }

            const auto set_bits_count_before = wheel.bitset(slot_index).count();
            auto       res                   = wheel.allocate(delay, data_t{i});
            SKL_ASSERT(res.first.slot_index == slot_index);
            const auto set_bits_count_after = wheel.bitset(slot_index).count();
            SKL_ASSERT(set_bits_count_after == set_bits_count_before + 1u);
            handles.push_back(res.first);
        }
        total_allocated_items += CAllocCount;

        while (false == wheel.tick(skl::get_current_epoch_time(), [&wheel_ticked_items](wheel_t::wheel_entry_t&) noexcept { ++wheel_ticked_items; })) { }

        ++test_ticks_count;
        skl::skl_busy_sleep(1000u / 144u);
    }

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount;) {
        i += u32(wheel.tick(skl::get_current_epoch_time(), [&wheel_ticked_items](wheel_t::wheel_entry_t&) noexcept { ++wheel_ticked_items; }));
        skl::skl_busy_sleep(wheel_t::CGranularityMs + 5u);
    }

    for (const auto& handle : handles) {
        SKL_ASSERT(false == wheel.validate_handle(handle));
    }

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount; ++i) {
        ASSERT_EQ(wheel.get_set_bits_count(i), 0u);
        ASSERT_EQ(wheel.slot(i).size(), 0u);
    }
    ASSERT_EQ(total_allocated_items, test_ticks_count * CAllocCount);
    ASSERT_EQ(wheel_ticked_items, total_allocated_items);
}

TEST(SkylakeTimerWheel, allocations_random_delay_and_disable_random) {
    wheel_t wheel{};

    std::vector<skl::timer_wheel_handle_t> handles;
    handles.reserve(1u << 17u);
    constexpr auto CAllocCount           = 50000u;
    u32            wheel_ticked_items    = 0u;
    u32            total_allocated_items = 0u;
    u32            test_ticks_count      = 0u;
    u32            total_disabled_items  = 0u;

    const auto start_time = skl::get_current_epoch_time();
    wheel.set_start_time();
    while ((skl::get_current_epoch_time() - start_time) <= 1500u) {
        handles.clear();

        for (u32 i = 0u; i < CAllocCount; ++i) {
            const auto delay      = skl::get_thread_rand().next_range(0u, wheel_t::CWheelTimeMs * 2u);
            u32        slot_index = wheel.pending_slot_index() + wheel.calculate_granularity_count(delay);
            if (slot_index >= wheel_t::CWheelSlotsCount) {
                slot_index -= wheel_t::CWheelSlotsCount;
            }

            const auto set_bits_count_before = wheel.bitset(slot_index).count();
            auto       res                   = wheel.allocate(delay, data_t{i});
            SKL_ASSERT(res.first.slot_index == slot_index);
            const auto set_bits_count_after = wheel.bitset(slot_index).count();
            SKL_ASSERT(set_bits_count_after == set_bits_count_before + 1u);
            handles.push_back(res.first);
        }
        total_allocated_items += CAllocCount;

        while (false == wheel.tick(skl::get_current_epoch_time(), [&wheel_ticked_items](wheel_t::wheel_entry_t&) noexcept { ++wheel_ticked_items; })) { }

        //Disable random entries
        const auto disable_count = skl::get_thread_rand().next_range(0u, 1000u);
        for (u32 i = 0u; i < disable_count; ++i) {
            const auto handle = handles[skl::get_thread_rand().next_range(0u, handles.size() - 1u)];
            const auto result = wheel.is_entry_enabled(handle);
            if (result.is_success()) {
                wheel.disable_entry(handle);
                ++total_disabled_items;
            }
        }

        ++test_ticks_count;
        skl::skl_busy_sleep(1000u / 144u);
    }

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount;) {
        i += u32(wheel.tick(skl::get_current_epoch_time(), [&wheel_ticked_items](wheel_t::wheel_entry_t&) noexcept { ++wheel_ticked_items; }));
        skl::skl_busy_sleep(wheel_t::CGranularityMs + 5u);
    }

    for (const auto& handle : handles) {
        SKL_ASSERT(false == wheel.validate_handle(handle));
    }

    for (u32 i = 0u; i < wheel_t::CWheelSlotsCount; ++i) {
        ASSERT_EQ(wheel.get_set_bits_count(i), 0u);
        ASSERT_EQ(wheel.slot(i).size(), 0u);
    }
    ASSERT_EQ(total_allocated_items, test_ticks_count * CAllocCount);
    ASSERT_EQ(wheel_ticked_items, total_allocated_items - total_disabled_items);
}
