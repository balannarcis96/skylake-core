#include <skl_spsc_bidirectional_ring>

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

spsc_bidirectional_ring_t<my_object_t, 1024U> g_queue{};
SKL_CACHE_ALIGNED std::relaxed_value<bool> g_run{true};
u64                                        g_order = 0ULL;

constexpr u64 CBurstSize = skl::skl_min<u64>(32ULL, g_queue.Size);

void spsc_bidirectional_ring_producer() noexcept {
    my_object_t* temp[CBurstSize];

    u64 processed_count = 0U;
    while (g_run) {
        //1. Allocate objects
        SKL_ASSERT_PERMANENT(g_queue.allocate_bulk(temp, std::size(temp)));

        //2. Prepare them
        u64 i = 0ULL;
        for (auto* obj : temp) {
            for (auto& number : obj->numbers) {
                number = i++;
            }
            SKL_ASSERT_PERMANENT(obj->is_free);
            obj->is_free = false;
            obj->order   = g_order++;
        }

        //3. Submit objects for processing
        g_queue.submit();

        //4. Burst results (in this case we wait for all to be processed)
        u32 burst_count = 0U;
        while (g_run && (burst_count < std::size(temp))) {
            burst_count += g_queue.dequeue_results_burst(temp + burst_count, std::size(temp) - burst_count);
        }

        if (false == g_run) {
            break;
        }

        //5. Assert expected order and process result
        i = 0ULL;
        for (auto* obj : temp) {
            for (auto& number : obj->numbers) {
                SKL_ASSERT_PERMANENT(number == CBurstSize + i++);
            }

            ++processed_count;

            SKL_ASSERT_PERMANENT(false == obj->is_free);
        }

        SKL_ASSERT_PERMANENT(temp[std::size(temp) - 1U]->order == (g_order - 1ULL));

        //6. Finally free objects
        g_queue.submit_processed_results();
    }

    (void)printf("[Producer] Completed (roundtrip) count: %lu\n", processed_count);
}

void spsc_bidirectional_ring_consumer() noexcept {
    my_object_t* temp[CBurstSize];

    while (g_run) {
        const auto count = g_queue.dequeue_burst(temp, std::size(temp));
        if (0U < count) {
            for (auto* obj : temp) {
                for (auto& number : obj->numbers) {
                    number += CBurstSize;
                }
            }

            g_queue.submit_results();
        }
    }
}
} // namespace

TEST(SkylakeSPSCBidirectionalRing, General) {
    ASSERT_TRUE(skl::skl_core_init().is_success());
    ASSERT_TRUE(skl::register_epilog_termination_handler([](i32) static noexcept {
        (void)g_run.exchange(false);
    }).is_success());

    skl::SKLThread producer{skl::skl_string_view::from_cstr("producer")};
    skl::SKLThread consumer{skl::skl_string_view::from_cstr("consumer")};

    producer.set_handler([](void) static noexcept -> i32 {
        spsc_bidirectional_ring_producer();
        return 0;
    });
    consumer.set_handler([](void) static noexcept -> i32 {
        spsc_bidirectional_ring_consumer();
        return 0;
    });

    ASSERT_TRUE(producer.create().is_success());
    ASSERT_TRUE(consumer.create().is_success());

    const auto start = skl::get_current_epoch_time();
    while ((skl::get_current_epoch_time() - start) < 5000U) {
        skl::skl_sleep(30U);
    }

    (void)g_run.exchange(false);

    ASSERT_TRUE(producer.join().is_success());
    ASSERT_TRUE(consumer.join().is_success());

    ASSERT_TRUE(skl::skl_core_deinit().is_success());
}
