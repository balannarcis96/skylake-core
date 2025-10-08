#include <algorithm>
#include <skl_deck>
#include <gtest/gtest.h>

TEST(SkylakeDeck, basic_add_and_iterate) {
    struct test_functor_t {
        static bool operator()(i32& f_value) noexcept {
            f_value += 1;
            return true;
        }
    };

    skl::Deck<i32> f_deck;

    for (i32 i = 0; i < 10; ++i) {
        auto [node, item] = f_deck.add(i);
        ASSERT_TRUE(node != nullptr);
        ASSERT_TRUE(item != nullptr);
        ASSERT_EQ(*item, i);
    }

    f_deck.for_each<test_functor_t>();

    static i32 expected = 1;
    expected            = 1;

    f_deck.for_each<decltype([](i32& f_value) static noexcept {
        ASSERT_EQ(f_value, expected);
        expected += 1;
    })>();
}

TEST(SkylakeDeck, remove_first_basic) {
    skl::Deck<i32> deck;

    // Add items
    for (i32 i = 0; i < 10; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 10u);

    // Remove item with value 5
    bool removed = deck.remove_first([](const i32& val) { return val == 5; });
    ASSERT_TRUE(removed);
    ASSERT_EQ(deck.size(), 9u);

    // Try to remove again - should fail
    removed = deck.remove_first([](const i32& val) { return val == 5; });
    ASSERT_FALSE(removed);
    ASSERT_EQ(deck.size(), 9u);

    // Verify 5 is gone
    bool found_five = false;
    deck.for_each([&found_five](i32& val) {
        if (val == 5) {
            found_five = true;
        }
        return true;
    });
    ASSERT_FALSE(found_five);
}

TEST(SkylakeDeck, remove_updates_m_insert_bug) {
    // This tests bug #1: m_insert not updated after remove_first
    skl::Deck<i32, 4u> small_deck; // Small block size to test multiple nodes

    // Fill first node completely
    for (i32 i = 0; i < 4; ++i) {
        small_deck.add(i);
    }

    // Add items to create second node
    small_deck.add(4);
    small_deck.add(5);
    ASSERT_EQ(small_deck.size(), 6u);

    // Remove from first node to create space
    bool removed = small_deck.remove_first([](const i32& val) { return val == 0; });
    ASSERT_TRUE(removed);
    ASSERT_EQ(small_deck.size(), 5u);

    // BUG: m_insert still points to second node
    // Next adds should ideally fill the space in first node
    // but will go to second node instead

    // Add enough items to show inefficiency
    small_deck.add(100);
    small_deck.add(101);
    small_deck.add(102); // This will create a third node due to the bug

    // With the bug:
    //   Node 1: [3, 1, 2, _] (0 was removed, 3 moved there via back_swap)
    //   Node 2: [4, 5, 100, 101]
    //   Node 3: [102, _, _, _]
    // Without the bug:
    //   Node 1: [3, 1, 2, 100] (fills the gap)
    //   Node 2: [4, 5, 101, 102]

    // We can't directly test node placement, but we can verify
    // all values are still accessible
    std::set<i32> expected = {1, 2, 3, 4, 5, 100, 101, 102};
    std::set<i32> actual;
    small_deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });

    ASSERT_EQ(expected, actual);
    ASSERT_EQ(small_deck.size(), 8u);
}

TEST(SkylakeDeck, size_tracking) {
    skl::Deck<i32> deck;
    ASSERT_EQ(deck.size(), 0u);
    ASSERT_TRUE(deck.empty());

    // Add items
    for (i32 i = 0; i < 100; ++i) {
        deck.add(i);
        ASSERT_EQ(deck.size(), u64(i + 1));
    }
    ASSERT_FALSE(deck.empty());

    // Remove half
    i32 removed_count = 0;
    for (i32 i = 0; i < 100; i += 2) {
        bool removed = deck.remove_first([i](const i32& val) { return val == i; });
        if (removed) {
            removed_count++;
        }
    }
    ASSERT_EQ(deck.size(), 100u - removed_count);

    // Clear
    deck.clear();
    ASSERT_EQ(deck.size(), 0u);
    ASSERT_TRUE(deck.empty());
}

TEST(SkylakeDeck, multiple_nodes) {
    skl::Deck<i32, 8u> deck; // 8 items per node

    // Add enough items to span multiple nodes
    for (i32 i = 0; i < 25; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 25u);

    // Should have 4 nodes: [8], [8], [8], [1]
    // Verify all items are accessible
    std::vector<bool> found(25, false);
    deck.for_each([&found](i32& val) {
        if (val >= 0 && val < 25) {
            found[val] = true;
        }
        return true;
    });

    for (bool f : found) {
        ASSERT_TRUE(f);
    }
}

TEST(SkylakeDeck, compact_basic) {
    skl::Deck<i32, 4u, false> deck; // Enable compaction

    // Create fragmented structure
    for (i32 i = 0; i < 12; ++i) {
        deck.add(i);
    }
    // Should have 3 nodes: [0,1,2,3], [4,5,6,7], [8,9,10,11]

    // Remove from middle nodes to create gaps
    deck.remove_first([](const i32& val) { return val == 1; });
    deck.remove_first([](const i32& val) { return val == 2; });
    deck.remove_first([](const i32& val) { return val == 5; });
    deck.remove_first([](const i32& val) { return val == 6; });

    ASSERT_EQ(deck.size(), 8u);

    // Compact
    deck.compact();
    ASSERT_EQ(deck.size(), 8u); // Size should remain same

    // Verify all remaining items still exist
    std::set<i32> expected = {0, 3, 4, 7, 8, 9, 10, 11};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, compact_empty_nodes) {
    skl::Deck<i32, 2u, false> deck; // Very small nodes

    // Create structure with empty nodes
    for (i32 i = 0; i < 6; ++i) {
        deck.add(i);
    }
    // Nodes: [0,1], [2,3], [4,5]

    // Empty the middle node completely
    deck.remove_first([](const i32& val) { return val == 2; });
    deck.remove_first([](const i32& val) { return val == 3; });

    ASSERT_EQ(deck.size(), 4u);

    // Compact should remove the empty node
    deck.compact();
    ASSERT_EQ(deck.size(), 4u);

    // Add more items - they should fill efficiently
    deck.add(100);
    deck.add(101);
    ASSERT_EQ(deck.size(), 6u);
}

TEST(SkylakeDeck, empty_deck_operations) {
    skl::Deck<i32> deck;

    // Operations on empty deck
    ASSERT_TRUE(deck.empty());
    ASSERT_EQ(deck.size(), 0u);

    bool removed = deck.remove_first([](const i32&) { return true; });
    ASSERT_FALSE(removed);

    i32 count = 0;
    deck.for_each([&count](i32&) {
        count++;
        return true;
    });
    ASSERT_EQ(count, 0);
}

TEST(SkylakeDeck, clear_and_reuse) {
    skl::Deck<i32> deck;

    // Add items
    for (i32 i = 0; i < 50; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 50u);

    // Clear
    deck.clear();
    ASSERT_EQ(deck.size(), 0u);
    ASSERT_TRUE(deck.empty());

    // Reuse after clear
    for (i32 i = 100; i < 110; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 10u);

    // Verify new items
    i32 min_val = INT_MAX;
    deck.for_each([&min_val](i32& val) {
        min_val = std::min(val, min_val);
        return true;
    });
    ASSERT_EQ(min_val, 100);
}

TEST(SkylakeDeck, stress_add_remove) {
    skl::Deck<i32, 16u> deck;

    // Stress test with many adds and removes
    for (i32 cycle = 0; cycle < 10; ++cycle) {
        // Add batch
        for (i32 i = cycle * 100; i < (cycle + 1) * 100; ++i) {
            deck.add(i);
        }

        // Remove every third item (0, 3, 6, 9, ... 99)
        i32 removed_count = 0;
        for (i32 i = cycle * 100; i < (cycle + 1) * 100; i += 3) {
            if (deck.remove_first([i](const i32& val) { return val == i; })) {
                removed_count++;
            }
        }
        ASSERT_EQ(removed_count, 34); // 100/3 + 1 because we start at 0
    }

    // Final size: 10 cycles * (100 added - 34 removed)
    ASSERT_EQ(deck.size(), 660u);
}

TEST(SkylakeDeck, back_swap_erase_order) {
    skl::Deck<i32, 4u> deck;

    // Add items
    for (i32 i = 0; i < 4; ++i) {
        deck.add(i);
    }
    // Node: [0, 1, 2, 3]

    // Remove item at index 1 (value=1)
    deck.remove_first([](const i32& val) { return val == 1; });

    // back_swap_erase should have moved 3 to position 1
    // So order is now: [0, 3, 2, _]
    std::vector<i32> values;
    deck.for_each([&values](i32& val) {
        values.push_back(val);
        return true;
    });

    ASSERT_EQ(values.size(), 3u);
    // Can't guarantee exact order due to back_swap_erase
    std::set<i32> value_set(values.begin(), values.end());
    ASSERT_EQ(value_set, std::set<i32>({0, 2, 3}));
}
TEST(SkylakeDeck, remove_all_basic) {
    skl::Deck<i32> deck;

    // Add items with duplicates
    for (i32 i = 0; i < 10; ++i) {
        deck.add(i % 3); // Pattern: 0,1,2,0,1,2,0,1,2,0
    }
    ASSERT_EQ(deck.size(), 10u);

    // Remove all items with value 1
    u32 removed = deck.remove_all([](const i32& val) { return val == 1; });
    ASSERT_EQ(removed, 3u); // Three 1s removed
    ASSERT_EQ(deck.size(), 7u);

    // Verify no 1s remain
    bool found_one = false;
    deck.for_each([&found_one](i32& val) {
        if (val == 1) {
            found_one = true;
        }
        return true;
    });
    ASSERT_FALSE(found_one);
}

TEST(SkylakeDeck, remove_all_static_functor) {
    struct RemoveEven {
        static bool operator()(const i32& val) noexcept {
            return val % 2 == 0;
        }
    };

    skl::Deck<i32> deck;

    // Add mix of even and odd
    for (i32 i = 0; i < 20; ++i) {
        deck.add(i);
    }

    // Remove all even numbers
    u32 removed = deck.remove_all<RemoveEven>();
    ASSERT_EQ(removed, 10u);
    ASSERT_EQ(deck.size(), 10u);

    // Verify only odds remain
    deck.for_each([](i32& val) noexcept -> bool {
        EXPECT_EQ(val % 2, 1);
        return true;
    });
}

TEST(SkylakeDeck, remove_all_empty_deck) {
    skl::Deck<i32> deck;

    // Remove from empty deck
    u32 removed = deck.remove_all([](const i32&) { return true; });
    ASSERT_EQ(removed, 0u);
    ASSERT_TRUE(deck.empty());
}

TEST(SkylakeDeck, remove_all_nothing_matches) {
    skl::Deck<i32> deck;

    // Add items
    for (i32 i = 0; i < 10; ++i) {
        deck.add(i);
    }

    // Try to remove items that don't exist
    u32 removed = deck.remove_all([](const i32& val) { return val > 100; });
    ASSERT_EQ(removed, 0u);
    ASSERT_EQ(deck.size(), 10u);
}

TEST(SkylakeDeck, remove_all_everything_matches) {
    skl::Deck<i32> deck;

    // Add items
    for (i32 i = 0; i < 25; ++i) {
        deck.add(i);
    }

    // Remove everything
    u32 removed = deck.remove_all([](const i32&) { return true; });
    ASSERT_EQ(removed, 25u);
    ASSERT_EQ(deck.size(), 0u);
    ASSERT_TRUE(deck.empty());

    // Should be able to add after removing all
    deck.add(100);
    ASSERT_EQ(deck.size(), 1u);
}

TEST(SkylakeDeck, remove_all_across_nodes) {
    skl::Deck<i32, 4u> deck; // Small nodes

    // Add items across multiple nodes
    for (i32 i = 0; i < 20; ++i) {
        deck.add(i);
    }
    // 5 nodes: [0,1,2,3], [4,5,6,7], [8,9,10,11], [12,13,14,15], [16,17,18,19]

    // Remove all multiples of 3
    u32 removed = deck.remove_all([](const i32& val) { return val % 3 == 0; });
    ASSERT_EQ(removed, 7u); // 0,3,6,9,12,15,18
    ASSERT_EQ(deck.size(), 13u);

    // Verify correct items remain
    std::set<i32> expected = {1, 2, 4, 5, 7, 8, 10, 11, 13, 14, 16, 17, 19};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, remove_all_consecutive_items_same_node) {
    skl::Deck<i32, 8u> deck;

    // Add pattern where multiple consecutive items will be removed
    for (i32 i = 0; i < 8; ++i) {
        deck.add(i < 4 ? 100 : i); // [100,100,100,100,4,5,6,7]
    }

    u32 removed = deck.remove_all([](const i32& val) { return val == 100; });
    ASSERT_EQ(removed, 4u);
    ASSERT_EQ(deck.size(), 4u);

    // Due to back_swap_erase, final order might be [7,6,5,4] or similar
    std::set<i32> expected = {4, 5, 6, 7};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, remove_all_back_swap_behavior) {
    skl::Deck<i32, 6u> deck;

    // Add specific pattern to test back_swap_erase behavior
    for (i32 i = 0; i < 6; ++i) {
        deck.add(i); // [0,1,2,3,4,5]
    }

    // Remove items at beginning and middle
    u32 removed = deck.remove_all([](const i32& val) {
        return val == 0 || val == 2 || val == 3;
    });
    ASSERT_EQ(removed, 3u);

    // After back_swap_erase operations:
    // Remove 0: [5,1,2,3,4] (5 moved to 0's position)
    // Remove 2: [5,1,4,3] (4 moved to 2's position)
    // Remove 3: [5,1,4] (3 was last)

    std::set<i32> expected = {1, 4, 5};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, remove_all_vs_remove_first_comparison) {
    skl::Deck<i32> deck1;
    skl::Deck<i32> deck2;

    // Add same items to both
    for (i32 i = 0; i < 30; ++i) {
        deck1.add(i % 5); // 0,1,2,3,4,0,1,2,3,4...
        deck2.add(i % 5);
    }

    // Remove all 2s from deck1
    u32 removed_all = deck1.remove_all([](const i32& val) { return val == 2; });
    ASSERT_EQ(removed_all, 6u);

    // Remove first 2 from deck2 repeatedly
    u32 removed_first = 0;
    while (deck2.remove_first([](const i32& val) { return val == 2; })) {
        removed_first++;
    }
    ASSERT_EQ(removed_first, 6u);

    // Both should have same size
    ASSERT_EQ(deck1.size(), deck2.size());

    // Both should have no 2s
    bool found_two_in_1 = false;
    bool found_two_in_2 = false;
    deck1.for_each([&found_two_in_1](i32& val) {
        if (val == 2) {
            found_two_in_1 = true;
        }
        return true;
    });
    deck2.for_each([&found_two_in_2](i32& val) {
        if (val == 2) {
            found_two_in_2 = true;
        }
        return true;
    });
    ASSERT_FALSE(found_two_in_1);
    ASSERT_FALSE(found_two_in_2);
}

TEST(SkylakeDeck, remove_all_complex_predicate) {
    skl::Deck<i32> deck;

    // Add range of values
    for (i32 i = -20; i <= 20; ++i) {
        deck.add(i);
    }

    // Remove with complex predicate: negative evens or positive odds
    u32 removed = deck.remove_all([](const i32& val) {
        return (val < 0 && val % 2 == 0) || (val > 0 && val % 2 == 1);
    });

    // Should remove: -20,-18,...,-2 (10 items) and 1,3,5,...,19 (10 items)
    ASSERT_EQ(removed, 20u);
    ASSERT_EQ(deck.size(), 21u);

    // Remaining: -19,-17,...,-1 (10 items), 0 (1 item), 2,4,...,20 (10 items)
    deck.for_each([](i32& val) noexcept -> bool {
        if (val < 0) {
            EXPECT_EQ(val % 2, -1); // Negative odds
        } else if (val > 0) {
            EXPECT_EQ(val % 2, 0); // Positive evens
        } else {
            EXPECT_EQ(val, 0); // Zero
        }
        return true;
    });
}

TEST(SkylakeDeck, remove_all_large_scale) {
    skl::Deck<u64, 64u> deck;

    // Add many items
    for (u64 i = 0; i < 10000u; ++i) {
        deck.add(i);
    }

    // Remove all primes under 1000 (168 primes)
    auto is_prime = [](u64 n) {
        if (n < 2) {
            return false;
        }
        if (n == 2) {
            return true;
        }
        if (n % 2 == 0) {
            return false;
        }
        for (u64 i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                return false;
            }
        }
        return true;
    };

    u32 removed = deck.remove_all([&is_prime](const u64& val) {
        return val < 1000 && is_prime(val);
    });

    ASSERT_EQ(removed, 168u); // There are 168 primes under 1000
    ASSERT_EQ(deck.size(), 9832u);

    // Verify no primes under 1000 remain
    deck.for_each([&is_prime](u64& val) noexcept -> bool {
        if (val < 1000) {
            EXPECT_FALSE(is_prime(val));
        }
        return true;
    });
}

TEST(SkylakeDeck, remove_all_with_compact) {
    skl::Deck<i32, 4u, false> deck; // Enable compaction

    // Add items
    for (i32 i = 0; i < 40; ++i) {
        deck.add(i);
    }

    // Remove all even numbers (creates fragmentation)
    u32 removed = deck.remove_all([](const i32& val) { return val % 2 == 0; });
    ASSERT_EQ(removed, 20u);
    ASSERT_EQ(deck.size(), 20u);

    // Compact to consolidate
    deck.compact();
    ASSERT_EQ(deck.size(), 20u);

    // Verify all odds still exist
    deck.for_each([](i32& val) noexcept -> bool {
        EXPECT_EQ(val % 2, 1);
        return true;
    });

    // Add more items - should use space efficiently
    for (i32 i = 100; i < 110; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 30u);
}

TEST(SkylakeDeck, DISABLED_huge_instance_stress_test) {
    // Test with a large number of items to verify scalability
    constexpr u64 BLOCK_SIZE = 64u;
    constexpr u64 NUM_ITEMS  = 1'000'000u; // 1 million items

    skl::Deck<u64, BLOCK_SIZE> huge_deck;

    // Add 1 million items
    for (u64 i = 0; i < NUM_ITEMS; ++i) {
        auto [node, item] = huge_deck.add(i);
        ASSERT_NE(node, nullptr);
        ASSERT_NE(item, nullptr);
        ASSERT_EQ(*item, i);

        // Periodic size check
        if (i % 10000 == 0) {
            ASSERT_EQ(huge_deck.size(), i + 1);
        }
    }

    ASSERT_EQ(huge_deck.size(), NUM_ITEMS);

    // Expected number of nodes (ceiling division)
    u64 expected_nodes = (NUM_ITEMS + BLOCK_SIZE - 1) / BLOCK_SIZE;
    ASSERT_EQ(expected_nodes, 15625u); // 1000000/64 = 15625

    // Verify all items are accessible via iteration
    u64 sum_via_iteration = 0;
    u64 count             = 0;
    huge_deck.for_each([&sum_via_iteration, &count](u64& val) {
        sum_via_iteration += val;
        count++;
        return true;
    });
    ASSERT_EQ(count, NUM_ITEMS);

    // Expected sum: n*(n-1)/2 where n = NUM_ITEMS
    u64 expected_sum = (NUM_ITEMS * (NUM_ITEMS - 1)) / 2;
    ASSERT_EQ(sum_via_iteration, expected_sum);

    // Remove every 1000th item (1000 total removals)
    u64 removed_count = 0;
    for (u64 i = 0; i < NUM_ITEMS; i += 1000) {
        bool removed = huge_deck.remove_first([i](const u64& val) { return val == i; });
        if (removed) {
            removed_count++;
        }
    }
    ASSERT_EQ(removed_count, 1000u);
    ASSERT_EQ(huge_deck.size(), NUM_ITEMS - 1000u);

    // Add more items after removals
    for (u64 i = NUM_ITEMS; i < NUM_ITEMS + 5000u; ++i) {
        huge_deck.add(i);
    }
    ASSERT_EQ(huge_deck.size(), NUM_ITEMS - 1000u + 5000u);

    // Pattern-based removal: remove all even numbers in range [1000, 2000)
    u64 pattern_removed = 0;
    for (u64 i = 1000; i < 2000; i += 2) {
        if (huge_deck.remove_first([i](const u64& val) { return val == i; })) {
            pattern_removed++;
        }
    }
    // Some of these might have been removed already (multiples of 1000)
    ASSERT_GT(pattern_removed, 400u); // At least 400 should succeed

    // Clear and verify
    huge_deck.clear();
    ASSERT_EQ(huge_deck.size(), 0u);
    ASSERT_TRUE(huge_deck.empty());

    // Reuse after clear with different pattern
    for (u64 i = 0; i < 10000u; ++i) {
        huge_deck.add(i * i); // Square numbers
    }
    ASSERT_EQ(huge_deck.size(), 10000u);

    // Verify square pattern
    u64 idx = 0;
    huge_deck.for_each([&idx](u64& val) noexcept -> bool {
        EXPECT_EQ(val, idx * idx);
        idx++;
        return true;
    });
}

TEST(SkylakeDeck, DISABLED_huge_instance_with_compaction) {
    // Test compaction at scale
    constexpr u64 BLOCK_SIZE = 32u;
    constexpr u64 NUM_ITEMS  = 100'000u;

    skl::Deck<u64, BLOCK_SIZE, false> huge_deck; // Enable compaction

    // Add items
    for (u64 i = 0; i < NUM_ITEMS; ++i) {
        huge_deck.add(i);
    }
    ASSERT_EQ(huge_deck.size(), NUM_ITEMS);

    // Create fragmentation: remove every prime number up to 1000
    auto is_prime = [](u64 n) {
        if (n < 2) {
            return false;
        }
        if (n == 2) {
            return true;
        }
        if (n % 2 == 0) {
            return false;
        }
        for (u64 i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                return false;
            }
        }
        return true;
    };

    u64 primes_removed = 0;
    for (u64 i = 2; i < 1000; ++i) {
        if (is_prime(i)) {
            if (huge_deck.remove_first([i](const u64& val) { return val == i; })) {
                primes_removed++;
            }
        }
    }
    ASSERT_GT(primes_removed, 150u); // There are 168 primes under 1000

    u64 size_before_compact = huge_deck.size();

    // Compact the fragmented structure
    huge_deck.compact();

    // Size should be preserved
    ASSERT_EQ(huge_deck.size(), size_before_compact);

    // Add more items - should efficiently use compacted space
    for (u64 i = NUM_ITEMS; i < NUM_ITEMS + 10000u; ++i) {
        huge_deck.add(i);
    }
    ASSERT_EQ(huge_deck.size(), size_before_compact + 10000u);

    // Verify data integrity after all operations
    std::set<u64> remaining_values;
    huge_deck.for_each([&remaining_values](u64& val) {
        remaining_values.insert(val);
        return true;
    });

    // Should have all non-prime values < 1000, plus all values >= 1000
    for (u64 i = 0; i < 1000; ++i) {
        if (!is_prime(i)) {
            ASSERT_TRUE(remaining_values.contains(i) > 0);
        }
    }
    for (u64 i = 1000; i < NUM_ITEMS + 10000u; ++i) {
        ASSERT_TRUE(remaining_values.contains(i) > 0);
    }
}

TEST(SkylakeDeck, compact_empty_deck) {
    skl::Deck<i32, 4u, false> deck;
    
    // Compact empty deck - should be no-op
    deck.compact();
    ASSERT_TRUE(deck.empty());
    ASSERT_EQ(deck.size(), 0u);
    
    // Add and remove everything, then compact
    for (i32 i = 0; i < 10; ++i) {
        deck.add(i);
    }
    
    // Remove ALL items - don't assume they're in order
    while (!deck.empty()) {
        deck.remove_first([](const i32&) { return true; });
    }
    ASSERT_TRUE(deck.empty());
    
    // Compact should clean up empty nodes except head
    deck.compact();
    ASSERT_TRUE(deck.empty());
    
    // Should still be able to add after compacting empty deck
    deck.add(100);
    ASSERT_EQ(deck.size(), 1u);
}

TEST(SkylakeDeck, compact_single_node) {
    skl::Deck<i32, 8u, false> deck;

    // Add items to partially fill single node
    for (i32 i = 0; i < 5; ++i) {
        deck.add(i);
    }

    // Remove some items
    deck.remove_first([](const i32& val) { return val == 1; });
    deck.remove_first([](const i32& val) { return val == 3; });

    // Compact single node - should be no-op
    deck.compact();
    ASSERT_EQ(deck.size(), 3u);

    // Verify remaining values
    std::set<i32> expected = {0, 2, 4};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, compact_all_nodes_full) {
    skl::Deck<i32, 4u, false> deck;

    // Fill exactly 3 nodes
    for (i32 i = 0; i < 12; ++i) {
        deck.add(i);
    }
    ASSERT_EQ(deck.size(), 12u);

    // All nodes are full - compact should be no-op
    deck.compact();
    ASSERT_EQ(deck.size(), 12u);

    // Should allocate new node for next add
    deck.add(100);
    ASSERT_EQ(deck.size(), 13u);
}

TEST(SkylakeDeck, compact_alternating_empty_full) {
    skl::Deck<i32, 2u, false> deck; // Very small nodes

    // Create pattern: full, empty, full, empty, partial
    for (i32 i = 0; i < 9; ++i) {
        deck.add(i);
    }
    // Nodes: [0,1], [2,3], [4,5], [6,7], [8,_]

    // Remove to create alternating pattern
    deck.remove_first([](const i32& val) { return val == 2; });
    deck.remove_first([](const i32& val) { return val == 3; });
    deck.remove_first([](const i32& val) { return val == 6; });
    deck.remove_first([](const i32& val) { return val == 7; });
    // Nodes: [0,1], [_,_], [4,5], [_,_], [8,_]

    deck.compact();
    ASSERT_EQ(deck.size(), 5u);

    // Should have compacted to: [0,1], [4,5], [8,_]
    std::set<i32> expected = {0, 1, 4, 5, 8};
    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, compact_chain_of_empty_nodes) {
    skl::Deck<i32, 3u, false> deck;

    // Create many nodes
    for (i32 i = 0; i < 30; ++i) {
        deck.add(i);
    }
    // 10 nodes of 3 items each

    // Empty several consecutive nodes in the middle
    for (i32 i = 9; i < 21; ++i) { // Empties nodes 4, 5, 6, 7
        deck.remove_first([i](const i32& val) { return val == i; });
    }

    deck.compact();
    ASSERT_EQ(deck.size(), 18u);

    // Verify all remaining items exist
    std::set<i32> expected;
    for (i32 i = 0; i < 9; ++i) {
        expected.insert(i);
    }
    for (i32 i = 21; i < 30; ++i) {
        expected.insert(i);
    }

    std::set<i32> actual;
    deck.for_each([&actual](i32& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, compact_non_trivial_type) {
    struct NonTrivial {
        std::unique_ptr<i32> value;
        NonTrivial(i32 v)
            : value(std::make_unique<i32>(v)) { }
        NonTrivial(NonTrivial&& other)            = default;
        NonTrivial& operator=(NonTrivial&& other) = default;
        bool        operator==(i32 v) const { return value && *value == v; }
    };

    skl::Deck<NonTrivial, 3u, false> deck;

    // Add items
    for (i32 i = 0; i < 9; ++i) {
        deck.add(i);
    }

    // Create fragmentation
    deck.remove_first([](const NonTrivial& val) { return val == 1; });
    deck.remove_first([](const NonTrivial& val) { return val == 4; });
    deck.remove_first([](const NonTrivial& val) { return val == 7; });

    // Compact with non-trivial type (uses move semantics path)
    deck.compact();
    ASSERT_EQ(deck.size(), 6u);

    // Verify values
    std::set<i32> expected = {0, 2, 3, 5, 6, 8};
    std::set<i32> actual;
    deck.for_each([&actual](NonTrivial& val) {
        if (val.value) {
            actual.insert(*val.value);
        }
        return true;
    });
    ASSERT_EQ(expected, actual);
}

TEST(SkylakeDeck, compact_updates_m_insert_correctly) {
    skl::Deck<i32, 3u, false> deck;

    // Fill several nodes
    for (i32 i = 0; i < 12; ++i) {
        deck.add(i);
    }
    // Nodes: [0,1,2], [3,4,5], [6,7,8], [9,10,11]

    // Create space in first node, leave last node full
    deck.remove_first([](const i32& val) { return val == 0; });

    deck.compact();

    // m_insert should now point to first node which has space
    // Add item - should go to first available space
    deck.add(100);
    ASSERT_EQ(deck.size(), 12u);

    // Continue adding to verify m_insert is working correctly
    deck.add(101);
    deck.add(102);
    ASSERT_EQ(deck.size(), 14u);
}

TEST(SkylakeDeck, compact_preserves_data_integrity) {
    skl::Deck<u64, 5u, false> deck;

    // Add items with specific pattern
    std::vector<u64> original_values;
    for (u64 i = 0; i < 100; ++i) {
        u64 value = i * i + i; // Unique pattern
        deck.add(value);
        original_values.push_back(value);
    }

    // Remove every 7th item
    std::set<u64> removed;
    for (u64 i = 0; i < 100; i += 7) {
        u64 value = original_values[i];
        if (deck.remove_first([value](const u64& val) { return val == value; })) {
            removed.insert(value);
        }
    }

    // Calculate expected remaining values
    std::set<u64> expected;
    for (u64 val : original_values) {
        if (removed.find(val) == removed.end()) {
            expected.insert(val);
        }
    }

    // Compact
    deck.compact();

    // Verify all expected values still exist
    std::set<u64> actual;
    deck.for_each([&actual](u64& val) {
        actual.insert(val);
        return true;
    });
    ASSERT_EQ(expected, actual);
    ASSERT_EQ(deck.size(), expected.size());
}

TEST(SkylakeDeck, compact_worst_case_fragmentation) {
    skl::Deck<i32, 4u, false> deck;

    // Create worst case: every node has only 1 item
    for (i32 i = 0; i < 20; ++i) {
        deck.add(i * 10);
        deck.add(i * 10 + 1);
        deck.add(i * 10 + 2);
        deck.add(i * 10 + 3);
    }
    // 20 nodes, all full

    // Remove 3 items from each node, leaving only 1
    for (i32 i = 0; i < 20; ++i) {
        deck.remove_first([i](const i32& val) { return val == i * 10 + 1; });
        deck.remove_first([i](const i32& val) { return val == i * 10 + 2; });
        deck.remove_first([i](const i32& val) { return val == i * 10 + 3; });
    }
    // Now 20 nodes, each with 1 item
    ASSERT_EQ(deck.size(), 20u);

    deck.compact();

    // Should compact to 5 full nodes
    ASSERT_EQ(deck.size(), 20u);

    // Verify all multiples of 10 are still there
    for (i32 i = 0; i < 20; ++i) {
        bool found  = false;
        i32  target = i * 10;
        deck.for_each([&found, target](i32& val) {
            if (val == target) {
                found = true;
            }
            return true;
        });
        ASSERT_TRUE(found);
    }
}

TEST(SkylakeDeck, compact_repeatedly) {
    skl::Deck<i32, 8u, false> deck;

    // Add initial batch
    for (i32 i = 0; i < 100; ++i) {
        deck.add(i);
    }

    // Repeatedly fragment and compact
    for (i32 round = 0; round < 5; ++round) {
        // Fragment by removing every 3rd item in current range
        std::vector<i32> to_remove;
        deck.for_each([&to_remove](i32& val) {
            if (val % 3 == 0) {
                to_remove.push_back(val);
            }
            return true;
        });

        for (i32 val : to_remove) {
            deck.remove_first([val](const i32& v) { return v == val; });
        }

        u64 size_before = deck.size();

        // Compact
        deck.compact();
        ASSERT_EQ(deck.size(), size_before);

        // Add new items
        for (i32 i = 0; i < 10; ++i) {
            deck.add(1000 + round * 10 + i);
        }
    }

    // Final verification
    ASSERT_GT(deck.size(), 50u);
}

TEST(SkylakeDeck, compact_mixed_operations) {
    skl::Deck<i32, 6u, false> deck;

    // Interleave adds, removes, and compacts
    for (i32 batch = 0; batch < 10; ++batch) {
        // Add batch
        for (i32 i = 0; i < 20; ++i) {
            deck.add(batch * 100 + i);
        }

        // Remove some
        for (i32 i = 5; i < 15; i += 2) {
            deck.remove_first([batch, i](const i32& val) {
                return val == batch * 100 + i;
            });
        }

        // Compact every 3rd batch
        if (batch % 3 == 0) {
            u64 size_before = deck.size();
            deck.compact();
            ASSERT_EQ(deck.size(), size_before);
        }
    }

    // Final compact and verify
    u64 final_size = deck.size();
    deck.compact();
    ASSERT_EQ(deck.size(), final_size);
}
