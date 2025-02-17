#include <gtest/gtest.h>

#include <skl_vector_if>

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

TEST(SKLVectorTests, skl_vector_basic_empty) {
    using vec_t = skl::skl_vector<TObjectTrivial>;
    vec_t vec{};

    ASSERT_TRUE(vec.valid());
    ASSERT_EQ(vec.size(), 0U);
    ASSERT_EQ(vec.capacity(), skl::CVectorIncreaseStep);
    ASSERT_NE(vec.data(), nullptr);
    ASSERT_DEATH((void)vec[0U], ".*");

    vec.clear();

    ASSERT_TRUE(vec.valid());
    ASSERT_EQ(vec.size(), 0U);
    ASSERT_EQ(vec.capacity(), skl::CVectorIncreaseStep);
    ASSERT_NE(vec.data(), nullptr);
    ASSERT_DEATH((void)vec[0U], ".*");

    {
        vec_t copy_ctor = vec;
        ASSERT_TRUE(copy_ctor.valid());
        ASSERT_EQ(copy_ctor.size(), 0U);
        ASSERT_EQ(copy_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(copy_ctor.data(), nullptr);
        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_NE(vec.data(), copy_ctor.data());
    }

    {
        vec_t copy_ctor = vec;
        ASSERT_TRUE(copy_ctor.valid());
        ASSERT_EQ(copy_ctor.size(), 0U);
        ASSERT_EQ(copy_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(copy_ctor.data(), nullptr);
        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_NE(vec.data(), copy_ctor.data());

        const auto* temp      = copy_ctor.data();
        vec_t       move_ctor = static_cast<vec_t&&>(copy_ctor);
        ASSERT_TRUE(move_ctor.valid());
        ASSERT_EQ(move_ctor.size(), 0U);
        ASSERT_EQ(move_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(move_ctor.data(), nullptr);
        ASSERT_DEATH((void)move_ctor[0U], ".*");
        ASSERT_EQ(temp, move_ctor.data());

        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_EQ(nullptr, copy_ctor.data());
        ASSERT_FALSE(copy_ctor.valid());
    }
}

TEST(SKLVectorTests, skl_vector) {
    using vec_t = skl::skl_vector<TObjectTrivial>;
    vec_t vec{};

    auto& vec_if = vec.upgrade();

    ASSERT_TRUE(vec_if.valid());
    ASSERT_EQ(vec_if.size(), 0U);
    ASSERT_EQ(vec_if.capacity(), skl::CVectorIncreaseStep);
    ASSERT_NE(vec_if.data(), nullptr);
    ASSERT_DEATH((void)vec_if[0U], ".*");

    ASSERT_EQ(vec_if.data(), vec.data());
    ASSERT_EQ(vec_if.size(), vec.size());
    ASSERT_EQ(vec_if.capacity(), vec.capacity());
    ASSERT_EQ(vec_if.valid(), vec.valid());

    vec_if.clear();

    ASSERT_TRUE(vec_if.valid());
    ASSERT_EQ(vec_if.size(), 0U);
    ASSERT_EQ(vec_if.capacity(), skl::CVectorIncreaseStep);
    ASSERT_NE(vec_if.data(), nullptr);
    ASSERT_DEATH((void)vec_if[0U], ".*");

    {
        vec_t copy_ctor = vec;
        ASSERT_TRUE(copy_ctor.valid());
        ASSERT_EQ(copy_ctor.size(), 0U);
        ASSERT_EQ(copy_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(copy_ctor.data(), nullptr);
        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_NE(vec_if.data(), copy_ctor.data());
    }

    {
        vec_t copy_ctor = vec;
        ASSERT_TRUE(copy_ctor.valid());
        ASSERT_EQ(copy_ctor.size(), 0U);
        ASSERT_EQ(copy_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(copy_ctor.data(), nullptr);
        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_NE(vec_if.data(), copy_ctor.data());

        const auto* temp      = copy_ctor.data();
        vec_t       move_ctor = static_cast<vec_t&&>(copy_ctor);
        ASSERT_TRUE(move_ctor.valid());
        ASSERT_EQ(move_ctor.size(), 0U);
        ASSERT_EQ(move_ctor.capacity(), skl::CVectorIncreaseStep);
        ASSERT_NE(move_ctor.data(), nullptr);
        ASSERT_DEATH((void)move_ctor[0U], ".*");
        ASSERT_EQ(temp, move_ctor.data());

        ASSERT_DEATH((void)copy_ctor[0U], ".*");
        ASSERT_EQ(nullptr, copy_ctor.data());
        ASSERT_FALSE(copy_ctor.valid());
    }
}

TEST(SKLVectorTests, skl_vector_2) {
    using vec_t = skl::skl_vector<TObjectTrivial>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    ASSERT_EQ(0U, vec_if.size());
    vec_if.push_back(TObjectTrivial{
        .a = 1,
        .b = 2,
        .c = 3});
    ASSERT_EQ(1U, vec_if.size());
    vec_if.push_back(TObjectTrivial{
        .a = 4,
        .b = 5,
        .c = 6});
    ASSERT_EQ(2U, vec_if.size());
    vec_if.push_back(TObjectTrivial{
        .a = 7,
        .b = 8,
        .c = 9});
    ASSERT_EQ(3U, vec_if.size());

    ASSERT_EQ(1, vec_if[0U].a);
    ASSERT_EQ(2, vec_if[0U].b);
    ASSERT_EQ(3, vec_if[0U].c);
    ASSERT_EQ(7, vec_if[2U].a);
    ASSERT_EQ(8, vec_if[2U].b);
    ASSERT_EQ(9, vec_if[2U].c);
    ASSERT_DEATH((void)vec_if[3U], ".*");

    vec_if.pop_back();
    ASSERT_EQ(2U, vec_if.size());
    ASSERT_DEATH((void)vec_if[2U], ".*");
    ASSERT_EQ(4, vec_if[1U].a);
    ASSERT_EQ(5, vec_if[1U].b);
    ASSERT_EQ(6, vec_if[1U].c);
}

TEST(SKLVectorTests, skl_vector_3) {
    using vec_t = skl::skl_vector<TObjectNonTrivial>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    const auto* buffer   = vec_if.data();
    const auto  capacity = vec_if.capacity();

    ASSERT_EQ(0U, vec_if.size());
    vec_if.push_back(TObjectNonTrivial{1});
    ASSERT_EQ(1U, vec_if.size());
    vec_if.push_back(TObjectNonTrivial{2});
    ASSERT_EQ(2U, vec_if.size());
    vec_if.push_back(TObjectNonTrivial{3});
    ASSERT_EQ(3U, vec_if.size());

    ASSERT_EQ(1, *vec_if[0U].d);
    ASSERT_EQ(2, *vec_if[1U].d);
    ASSERT_EQ(3, *vec_if[2U].d);
    ASSERT_DEATH((void)vec_if[3U], ".*");

    vec_if.pop_back();
    ASSERT_EQ(2U, vec_if.size());
    ASSERT_DEATH((void)vec_if[2U], ".*");
    ASSERT_EQ(1, *vec_if[0U].d);
    ASSERT_EQ(2, *vec_if[1U].d);

    vec_if.pop_back();
    ASSERT_EQ(1U, vec_if.size());
    ASSERT_DEATH((void)vec_if[1U], ".*");
    ASSERT_EQ(1, *vec_if[0U].d);

    vec_if.pop_back();
    ASSERT_EQ(0U, vec_if.size());
    ASSERT_DEATH((void)vec_if[0U], ".*");

    ASSERT_EQ(buffer, vec_if.data());
    ASSERT_EQ(capacity, vec_if.capacity());
}

TEST(SKLVectorTests, skl_vector_4) {
    using vec_t = skl::skl_vector<TObjectNonTrivial>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    auto* buffer   = vec_if.data();
    auto  capacity = vec_if.capacity();

    constexpr i32 CElementsCount = 4096;

    for (i32 i = 0; i < CElementsCount; ++i) {
        vec_if.push_back(TObjectNonTrivial{i});
    }

    ASSERT_EQ(CElementsCount, vec_if.size());
    ASSERT_EQ(CElementsCount - 1, *vec_if.back().d);

    for (i32 i = 0; i < CElementsCount; ++i) {
        ASSERT_EQ(i, *vec_if[i].d);
    }

    ASSERT_NE(buffer, vec_if.data());
    ASSERT_NE(capacity, vec_if.capacity());

    buffer   = vec_if.data();
    capacity = vec_if.capacity();

    vec_if.clear();
    ASSERT_EQ(0U, vec_if.size());
    ASSERT_EQ(buffer, vec_if.data());
    ASSERT_EQ(capacity, vec_if.capacity());
}

TEST(SKLVectorTests, skl_vector_5) {
    using vec_t = skl::skl_vector<TObjectNonTrivial>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    static constexpr i32 CElementsCount = 4096;

    auto& vec_if = vec.upgrade();
    for (i32 i = 0; i < CElementsCount; ++i) {
        vec_if.push_back(TObjectNonTrivial{i});
    }

    auto common_test = [](vec_t& target) noexcept {
        auto& vec_if = target.upgrade();

        ASSERT_EQ(CElementsCount, vec_if.size());
        ASSERT_EQ(CElementsCount - 1, *vec_if.back().d);

        for (i32 i = 0; i < CElementsCount; ++i) {
            ASSERT_EQ(i, *vec_if[i].d);
        }
    };

    common_test(vec);
    vec_t deep_copy = vec;
    common_test(deep_copy);
    deep_copy = vec;
    common_test(deep_copy);
}

TEST(SKLVectorTests, skl_vector_6) {
    using vec_t = skl::skl_vector<TObjectNonTrivial>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    auto* buffer = vec_if.data();

    constexpr i32 CElementsCount = 32;

    for (i32 i = 0; i < CElementsCount; ++i) {
        vec_if.push_back(TObjectNonTrivial{i});
    }

    vec_t vec2{};
    auto& vec_if2 = vec2.upgrade();

    for (i32 i = 0; i < CElementsCount; ++i) {
        vec_if2.push_back(TObjectNonTrivial{i});
    }

    auto* buffer2   = vec_if2.data();
    auto  capacity2 = vec_if2.capacity();

    vec_if2.clear();

    ASSERT_EQ(0U, vec_if2.size());
    ASSERT_EQ(buffer2, vec_if2.data());
    ASSERT_EQ(capacity2, vec_if2.capacity());
    ASSERT_NE(buffer, vec_if2.data());

    vec2 = vec;

    ASSERT_EQ(CElementsCount, vec_if2.size());
    ASSERT_EQ(buffer2, vec_if2.data());
    ASSERT_EQ(capacity2, vec_if2.capacity());
}

TEST(SKLVectorTests, skl_vector_empalce) {
    using vec_t = skl::skl_vector<TObjectOnlyMoveCtor>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    TObjectOnlyMoveCtor temp{555};
    ASSERT_NE(nullptr, temp.d);

    const auto* storage = temp.d;

    vec_if.emplace_back(static_cast<TObjectOnlyMoveCtor&&>(temp));
    ASSERT_EQ(1U, vec_if.size());
    ASSERT_EQ(storage, vec_if.front().d);
    ASSERT_EQ(nullptr, temp.d);
}

TEST(SKLVectorTests, skl_vector_resize) {
    static i32 Counter = 0;
    struct TempType {
        TempType() noexcept
            : data{++Counter} {
        }
        ~TempType() noexcept {
            --Counter;
        }
        const i32 data;
    };

    using vec_t = skl::skl_vector<TempType>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();

    vec_if.resize(32U);

    ASSERT_TRUE(32U == vec_if.size());
    ASSERT_TRUE(32U < vec_if.capacity());
    ASSERT_EQ(32U, Counter);

    vec_if.resize(16U);

    ASSERT_TRUE(16U == vec_if.size());
    ASSERT_TRUE(16U < vec_if.capacity());
    ASSERT_EQ(16, Counter);

    vec_if.resize(1024U);

    ASSERT_TRUE(1024U == vec_if.size());
    ASSERT_TRUE(1024U < vec_if.capacity());
    ASSERT_EQ(1024U, Counter);

    vec_if.resize(0U);

    ASSERT_TRUE(0U == vec_if.size());
    ASSERT_TRUE(0U < vec_if.capacity());
    ASSERT_EQ(0, Counter);

    vec_if.resize(1024U);

    ASSERT_TRUE(1024U == vec_if.size());
    ASSERT_TRUE(1024U < vec_if.capacity());
    ASSERT_EQ(1024U, Counter);
}

TEST(SKLVectorTests, skl_vector_find) {
    static i32 Counter = 0;
    struct TempType {
        TempType() noexcept
            : data{Counter++} {
        }
        explicit TempType(i32 data) noexcept
            : data{data} {
        }
        ~TempType() noexcept {
            --Counter;
        }

        [[nodiscard]] bool operator==(const TempType& f_other) const noexcept {
            return data == f_other.data;
        }

        const i32 data;
    };

    using vec_t = skl::skl_vector<TempType>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();
    vec_if.resize(32U);

    {

        const auto* result = vec_if.find_if_static<decltype([](const TempType& f_it) static noexcept {
            return f_it.data == 5U;
        })>();
        ASSERT_EQ(result, &vec[5U]);
    }

    {
        const auto* result = vec_if.find_if_static<decltype([](const TempType& f_it) static noexcept {
            return f_it.data == 55U;
        })>();
        ASSERT_EQ(vec_if.end(), result);
    }

    {
        const auto* result = vec_if.find_if([](const TempType& f_it) static noexcept {
            return f_it.data == 5U;
        });
        ASSERT_EQ(result, &vec[5U]);
    }

    {
        const auto* result = vec_if.find_if([](const TempType& f_it) static noexcept {
            return f_it.data == 55U;
        });
        ASSERT_EQ(vec_if.end(), result);
    }

    {
        const auto* result = vec_if.find(TempType(5));
        ASSERT_EQ(result, &vec[5U]);
    }

    {
        const auto* result = vec_if.find(TempType(55));
        ASSERT_EQ(vec_if.end(), result);
    }
}

TEST(SKLVectorTests, skl_vector_reserve) {
    static i32 Counter = 0;
    struct TempType {
        TempType() noexcept
            : data{Counter++} {
        }
        explicit TempType(i32 data) noexcept
            : data{data} {
        }
        ~TempType() noexcept {
            --Counter;
        }

        [[nodiscard]] bool operator==(const TempType& f_other) const noexcept {
            return data == f_other.data;
        }

        const i32 data;
    };

    using vec_t = skl::skl_vector<TempType>;
    vec_t vec{};
    ASSERT_TRUE(vec.valid());

    auto& vec_if = vec.upgrade();
    ASSERT_EQ(0, Counter);
    ASSERT_TRUE(32U > vec_if.capacity());

    vec_if.reserve(32U);

    ASSERT_EQ(0, Counter);
    ASSERT_TRUE(32U <= vec_if.capacity());

    auto old_capacity = vec_if.capacity();

    vec_if.reserve(32U);

    ASSERT_EQ(0, Counter);
    ASSERT_EQ(old_capacity, vec_if.capacity());

    vec_if.reserve(62U);

    ASSERT_EQ(0, Counter);
    ASSERT_TRUE(62U <= vec_if.capacity());
}
