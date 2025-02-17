#include <gtest/gtest.h>

#include <skl_circular_queue_if>

#include <mimalloc-new-delete.h>

struct TObjectTrivial {
    i32 a;
    i32 b;
    i32 c;
};
static_assert(__is_trivial(TObjectTrivial));

struct TObjectNonTrivial {
    TObjectNonTrivial(i32 val = 23) noexcept
        : d{new i32{val}} { }
    ~TObjectNonTrivial() noexcept {
        if (nullptr == d) {
            delete d;
            d = nullptr;
        }
    }
    TObjectNonTrivial(const TObjectNonTrivial& other) noexcept
        : d{new i32{*other.d}} {
    }
    TObjectNonTrivial& operator=(const TObjectNonTrivial& other) noexcept = delete;

    TObjectNonTrivial(TObjectNonTrivial&& other) noexcept
        : d{other.d} {
        other.d = nullptr;
    }
    TObjectNonTrivial& operator=(TObjectNonTrivial&& other) noexcept = delete;

    i32* d;
};
static_assert(false == __is_trivial(TObjectNonTrivial));

struct TObjectOnlyMoveCtor {
    TObjectOnlyMoveCtor(i32 val = 23) noexcept
        : d{new i32{val}} { }
    ~TObjectOnlyMoveCtor() noexcept {
        if (nullptr == d) {
            delete d;
            d = nullptr;
        }
    }
    TObjectOnlyMoveCtor(const TObjectOnlyMoveCtor& other) noexcept            = delete;
    TObjectOnlyMoveCtor& operator=(const TObjectOnlyMoveCtor& other) noexcept = delete;
    TObjectOnlyMoveCtor& operator=(TObjectOnlyMoveCtor&& other) noexcept      = delete;

    TObjectOnlyMoveCtor(TObjectOnlyMoveCtor&& other) noexcept
        : d{other.d} {
        other.d = nullptr;
    }

    i32* d;
};
static_assert(false == __is_trivial(TObjectOnlyMoveCtor));

TEST(SKLCircularQueueTests, skl_circular_queue_basics) {
    using queue_t = skl::skl_circular_queue<TObjectTrivial, 8U>;
    queue_t queue{};
    auto&   _if = queue.upgrade();

    ASSERT_EQ(queue.size(), 0U);
    ASSERT_EQ(queue.capacity(), 8U);
    ASSERT_NE(queue.data(), nullptr);
    ASSERT_DEATH((void)queue[0U], ".*");

    queue.clear();

    ASSERT_EQ(queue.size(), 0U);
    ASSERT_EQ(queue.capacity(), 8U);
    ASSERT_NE(queue.data(), nullptr);
    ASSERT_DEATH((void)queue[0U], ".*");

    for (auto k = 0; k < 5; ++k) {
        for (auto i = 0; i < 8U; ++i) {
            ASSERT_TRUE(_if.try_push({}));
            ASSERT_EQ(i + 1, _if.size());
        }

        _if.clear();

        ASSERT_DEATH((void)queue.front(), ".*");
        ASSERT_DEATH((void)queue.tail(), ".*");
    }

    for (auto i = 0; i < 8U; ++i) {
        ASSERT_TRUE(_if.try_push({}));
        ASSERT_EQ(i + 1, _if.size());
    }

    ASSERT_FALSE(_if.try_push({}));
    ASSERT_DEATH((void)_if.push({}), ".*");
}

TEST(SKLCircularQueueTests, skl_circular_queue_basics2) {
    using queue_t = skl::skl_circular_queue<TObjectTrivial, 8U>;
    queue_t queue{};
    auto&   _if = queue.upgrade();

    for (auto i = 0; i < 8U; ++i) {
        ASSERT_TRUE(_if.try_push({.a = i}));
        ASSERT_EQ(i + 1, _if.size());
    }

    for (auto i = 0; i < 8U; ++i) {
        ASSERT_EQ(i, queue[i].a);
    }

    ASSERT_FALSE(_if.try_push({}));
    ASSERT_DEATH((void)_if.push({}), ".*");

    i32 k = 7;
    while (false == queue.empty()) {
        ASSERT_EQ(queue.front().a, k);
        --k;
        _if.pop_front();
    }

    for (auto i = 0; i < 8U; ++i) {
        ASSERT_TRUE(_if.try_push({.a = i}));
        ASSERT_EQ(i + 1, _if.size());
    }

    k = 0;
    while (false == queue.empty()) {
        ASSERT_EQ(queue.tail().a, k);
        ++k;
        _if.pop();
    }

    for (i32 i = 0; i < 9; ++i) {
        ASSERT_FALSE(_if.try_pop());
        ASSERT_FALSE(_if.try_pop_front());
        ASSERT_DEATH(_if.pop(), ".*");
        ASSERT_DEATH(_if.pop_front(), ".*");
    }

    queue.clear();

    for (auto i = 0; i < 8U; ++i) {
        ASSERT_TRUE(_if.try_push({.a = 1234}));
        ASSERT_EQ(i + 1, _if.size());
    }

    ASSERT_TRUE(queue.full());

    for (i32 i = 0; i < 5123; ++i) {
        ASSERT_EQ(_if.tail().a, 1234);
        ASSERT_EQ(_if.front().a, 1234);
        ASSERT_EQ(queue.tail().a, 1234);
        ASSERT_EQ(queue.front().a, 1234);
        ASSERT_EQ(&queue.front(), &_if.front());
        ASSERT_EQ(&queue.tail(), &_if.tail());

        //Pop from tail
        _if.pop();

        //Push to front
        _if.push({.a = 1234});
    }

    ASSERT_FALSE(_if.try_push({}));
    ASSERT_DEATH((void)_if.push({}), ".*");

    ASSERT_TRUE(queue.full());
}
