#include <skl_dod_fsm>

#include <gtest/gtest.h>

// Test states enum
enum class ETestStates : u32 {
    Idle = 0,
    Moving,
    Attacking,
    Dead,
};

// Settings for test FSM
constexpr skl::dod_fsm::SklDoDFsmToolkitSettings CTestSettings{
    .default_accumulate_realtime_time = false,
    .invalid_entity_id                = 0u,
};

using test_toolkit_t = skl::dod_fsm::SklDoDFsmToolkit<ETestStates, CTestSettings>;

// State data types
struct idle_state_t {
    static constexpr ETestStates CState = ETestStates::Idle;

    struct transition_data_t {
        float initial_delay;
    };

    // Constructor from transition data
    idle_state_t() = default;
    explicit idle_state_t(const transition_data_t& f_trans) : wait_time(f_trans.initial_delay) {}

    float wait_time;
};

struct moving_state_t {
    static constexpr ETestStates CState = ETestStates::Moving;

    struct transition_data_t {
        float speed;
        float target_x;
        float target_y;
    };

    // Constructor from transition data
    moving_state_t() = default;
    explicit moving_state_t(const transition_data_t& f_trans)
        : speed(f_trans.speed), target_x(f_trans.target_x), target_y(f_trans.target_y), current_x(0.0f), current_y(0.0f) {}

    float speed;
    float target_x;
    float target_y;
    float current_x;
    float current_y;
};

struct attacking_state_t {
    static constexpr ETestStates CState = ETestStates::Attacking;

    struct transition_data_t {
        u32 target_id;
        u32 damage;
    };

    // Constructor from transition data
    attacking_state_t() = default;
    explicit attacking_state_t(const transition_data_t& f_trans)
        : target_id(f_trans.target_id), damage(f_trans.damage), attacks_performed(0) {}

    u32 target_id;
    u32 damage;
    u32 attacks_performed;
};

struct dead_state_t {
    static constexpr ETestStates CState = ETestStates::Dead;

    struct transition_data_t {
        float time_of_death;
    };

    // Constructor from transition data
    dead_state_t() = default;
    explicit dead_state_t(const transition_data_t& f_trans) : time_of_death(f_trans.time_of_death) {}

    float time_of_death;
};

// FSM instance
using test_fsm_t = test_toolkit_t::FSM<1, idle_state_t, moving_state_t, attacking_state_t, dead_state_t>;

// Tick handlers
namespace skl::dod_fsm {
template <>
ETestStates tick_state<ETestStates::Idle>(test_toolkit_t::state_instance_t<idle_state_t>& f_state_instance, float, float) noexcept {
    // [DOD] Idle state tick handler
    if (f_state_instance.header.accumulated_time_ms > f_state_instance.data.wait_time) {
        // Transition to Moving after wait
        moving_state_t::transition_data_t trans_data{
            .speed    = 10.0f,
            .target_x = 100.0f,
            .target_y = 100.0f,
        };
        test_fsm_t::enqueue_transition<moving_state_t>(f_state_instance.header.id, std::move(trans_data));
        return ETestStates::Moving;
    }
    return ETestStates::Idle;
}

template <>
ETestStates tick_state<ETestStates::Moving>(test_toolkit_t::state_instance_t<moving_state_t>& f_state_instance, float f_dt, float) noexcept {
    // [DOD] Moving state tick handler
    auto& data = f_state_instance.data;

    float dx   = data.target_x - data.current_x;
    float dy   = data.target_y - data.current_y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 1.0f) {
        // Reached target, transition to Attacking
        attacking_state_t::transition_data_t trans_data{
            .target_id = 999,
            .damage    = 50,
        };
        test_fsm_t::enqueue_transition<attacking_state_t>(f_state_instance.header.id, std::move(trans_data));
        return ETestStates::Attacking;
    }

    // Move towards target
    data.current_x += (dx / dist) * data.speed * f_dt;
    data.current_y += (dy / dist) * data.speed * f_dt;

    return ETestStates::Moving;
}

template <>
ETestStates tick_state<ETestStates::Attacking>(test_toolkit_t::state_instance_t<attacking_state_t>& f_state_instance, float, float f_total_time) noexcept {
    // [DOD] Attacking state tick handler
    auto& data = f_state_instance.data;
    data.attacks_performed++;

    if (data.attacks_performed >= 3) {
        // Die after 3 attacks
        dead_state_t::transition_data_t trans_data{
            .time_of_death = f_total_time,
        };
        test_fsm_t::enqueue_transition<dead_state_t>(f_state_instance.header.id, std::move(trans_data));
        return ETestStates::Dead;
    }

    return ETestStates::Attacking;
}

template <>
ETestStates tick_state<ETestStates::Dead>(test_toolkit_t::state_instance_t<dead_state_t>& f_state_instance, float, float) noexcept {
    // [DOD] Dead state tick handler - no transitions
    (void)f_state_instance; // Unused
    return ETestStates::Dead;
}

} // namespace skl::dod_fsm

// [TestUtil] Test fixture
class skylake_dod_fsm_test_t : public ::testing::Test {
protected:
    void SetUp() override {
        // Fresh start for each test
        test_fsm_t::clear();
        test_fsm_t::reset_timer();
    }

    void TearDown() override {
        // Clean up
    }
};

TEST_F(skylake_dod_fsm_test_t, basic_construction) {
    // [TestUtil] FSM should be constructible and tick without entities
    EXPECT_NO_THROW(test_fsm_t::tick());
}

TEST_F(skylake_dod_fsm_test_t, add_single_entity) {
    // [TestUtil] Add one entity to Idle state
    idle_state_t::transition_data_t trans_data{
        .initial_delay = 1000.0f,
    };

    const u32 entity_id = 1;
    test_fsm_t::enqueue_transition<idle_state_t>(entity_id, std::move(trans_data));

    // Process transition
    test_fsm_t::tick();

    // Verify entity exists and is in Idle state
    EXPECT_EQ(test_fsm_t::get_entity_state(entity_id), ETestStates::Idle);

    // Verify entity is in the instances vector
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), 1u);
    EXPECT_EQ(idle_instances[0].header.id, entity_id);
}

TEST_F(skylake_dod_fsm_test_t, remove_entity) {
    // [TestUtil] Test entity removal
    const u32                       entity_id = 1;
    idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
    test_fsm_t::enqueue_transition<idle_state_t>(entity_id, std::move(trans_data));
    test_fsm_t::tick();

    // Remove entity
    test_fsm_t::remove_entity(entity_id);
    test_fsm_t::tick();

    // Verify entity is gone
    EXPECT_EQ(test_fsm_t::get_entity_state(entity_id), static_cast<ETestStates>(0));
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), 0u);
}

TEST_F(skylake_dod_fsm_test_t, multiple_entities) {
    // [TestUtil] Test multiple entities
    for (u32 i = 1; i <= 10; ++i) {
        idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
        test_fsm_t::enqueue_transition<idle_state_t>(i, std::move(trans_data));
    }

    test_fsm_t::tick();

    // Verify all entities exist
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), 10u);

    for (u32 i = 1; i <= 10; ++i) {
        EXPECT_EQ(test_fsm_t::get_entity_state(i), ETestStates::Idle);
    }
}

TEST_F(skylake_dod_fsm_test_t, state_transition) {
    // [TestUtil] Test state transitions
    const u32                       entity_id = 1;
    idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
    test_fsm_t::enqueue_transition<idle_state_t>(entity_id, std::move(trans_data));
    test_fsm_t::tick();

    // Set wait time to trigger transition
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    ASSERT_EQ(idle_instances.size(), 1u);
    idle_instances[0].data.wait_time = 0.0f;

    // Tick to trigger transition (any accumulated time > 0.0f will trigger)
    test_fsm_t::tick();

    // Entity should have transitioned to Moving
    EXPECT_EQ(test_fsm_t::get_entity_state(entity_id), ETestStates::Moving);

    // Idle should be empty, Moving should have 1
    EXPECT_EQ(idle_instances.size(), 0u);
    auto& moving_instances = test_fsm_t::get_state_instances_vector<moving_state_t>();
    EXPECT_EQ(moving_instances.size(), 1u);
    EXPECT_EQ(moving_instances[0].header.id, entity_id);
}

TEST_F(skylake_dod_fsm_test_t, remove_multiple_entities) {
    // [TestUtil] Test removing multiple entities with swap-and-pop
    for (u32 i = 1; i <= 20; ++i) {
        idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
        test_fsm_t::enqueue_transition<idle_state_t>(i, std::move(trans_data));
    }
    test_fsm_t::tick();

    // Remove every other entity
    for (u32 i = 2; i <= 20; i += 2) {
        test_fsm_t::remove_entity(i);
    }
    test_fsm_t::tick();

    // Verify only odd entities remain
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), 10u);

    for (u32 i = 1; i <= 20; ++i) {
        if (i % 2 == 1) {
            EXPECT_EQ(test_fsm_t::get_entity_state(i), ETestStates::Idle);
        } else {
            EXPECT_EQ(test_fsm_t::get_entity_state(i), static_cast<ETestStates>(0));
        }
    }
}

TEST_F(skylake_dod_fsm_test_t, accumulated_time) {
    // [TestUtil] Test accumulated time tracking
    const u32                       entity_id = 1;
    idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
    test_fsm_t::enqueue_transition<idle_state_t>(entity_id, std::move(trans_data));
    test_fsm_t::tick();

    // Tick multiple times
    for (int i = 0; i < 10; ++i) {
        test_fsm_t::tick();
    }

    // Accumulated time should be > 0
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    ASSERT_EQ(idle_instances.size(), 1u);
    EXPECT_GT(idle_instances[0].header.accumulated_time_ms, 0.0f);
}

TEST_F(skylake_dod_fsm_test_t, swap_and_pop_preserves_entities) {
    // [TestUtil] [DOD] Test that swap-and-pop doesn't skip entities
    const u32 CNumEntities = 100;
    for (u32 i = 1; i <= CNumEntities; ++i) {
        idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
        test_fsm_t::enqueue_transition<idle_state_t>(i, std::move(trans_data));
    }
    test_fsm_t::tick();

    // Remove entities from the middle
    for (u32 i = 40; i <= 60; ++i) {
        test_fsm_t::remove_entity(i);
    }
    test_fsm_t::tick();

    // Verify remaining entities are still accessible
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), CNumEntities - 21);

    // Check that all remaining entities have valid states
    for (const auto& instance : idle_instances) {
        EXPECT_NE(instance.header.id, 0u);
        EXPECT_EQ(test_fsm_t::get_entity_state(instance.header.id), ETestStates::Idle);
    }
}

TEST_F(skylake_dod_fsm_test_t, transition_data_preserved) {
    // [TestUtil] Test that transition data is correctly copied
    const u32                         entity_id = 1;
    moving_state_t::transition_data_t trans_data{
        .speed    = 42.0f,
        .target_x = 123.0f,
        .target_y = 456.0f,
    };
    test_fsm_t::enqueue_transition<moving_state_t>(entity_id, std::move(trans_data));
    test_fsm_t::tick();

    // Verify transition data was copied correctly
    auto& moving_instances = test_fsm_t::get_state_instances_vector<moving_state_t>();
    ASSERT_EQ(moving_instances.size(), 1u);
    EXPECT_EQ(moving_instances[0].data.speed, 42.0f);
    EXPECT_EQ(moving_instances[0].data.target_x, 123.0f);
    EXPECT_EQ(moving_instances[0].data.target_y, 456.0f);
}

TEST_F(skylake_dod_fsm_test_t, no_entity_skipping) {
    // [TestUtil] [DOD] Ensure reverse iteration doesn't skip entities
    const u32 CNumEntities = 50;
    for (u32 i = 1; i <= CNumEntities; ++i) {
        idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
        test_fsm_t::enqueue_transition<idle_state_t>(i, std::move(trans_data));
    }
    test_fsm_t::tick();

    // Track which entities got ticked by incrementing accumulated time
    test_fsm_t::tick();

    // All entities should have accumulated time > 0
    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    for (const auto& instance : idle_instances) {
        EXPECT_GT(instance.header.accumulated_time_ms, 0.0f)
            << "Entity " << instance.header.id << " was not ticked";
    }
}

TEST_F(skylake_dod_fsm_test_t, avx512_batch_processing) {
    // [TestUtil] [DOD] Test with entity count that triggers batching
    const u32 CNumEntities = 64; // Multiple of 16 for AVX-512
    for (u32 i = 1; i <= CNumEntities; ++i) {
        idle_state_t::transition_data_t trans_data{.initial_delay = 1000.0f};
        test_fsm_t::enqueue_transition<idle_state_t>(i, std::move(trans_data));
    }
    test_fsm_t::tick();

    // Tick and verify all processed
    test_fsm_t::tick();

    auto& idle_instances = test_fsm_t::get_state_instances_vector<idle_state_t>();
    EXPECT_EQ(idle_instances.size(), CNumEntities);

    for (const auto& instance : idle_instances) {
        EXPECT_GT(instance.header.accumulated_time_ms, 0.0f);
    }
}
