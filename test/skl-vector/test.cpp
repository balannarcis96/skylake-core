#include <skl_vector_if>
#include <skl_log>

#include <memory>

#include <gtest/gtest.h>

#if !SKL_BUILD_SHIPPING
TEST(SkylakeVector, Basics_NoAllocateOnDefaultCtor_DefaultCtor) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>, skl::CVectorIncreaseStep>;

    using test_predicate_t = decltype([](vec_t& f_vec) static {
        ASSERT_FALSE(f_vec.has_storage());
        ASSERT_DEATH((void)f_vec.front(), ".*");
        ASSERT_DEATH((void)f_vec.back(), ".*");
        ASSERT_DEATH((void)f_vec[0U], ".*");
        ASSERT_TRUE(f_vec.empty());
        ASSERT_EQ(f_vec.size(), 0U);
        ASSERT_EQ(f_vec.capacity(), 0U);
        ASSERT_EQ(f_vec.begin(), nullptr);
        ASSERT_EQ(f_vec.end(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(f_vec).begin(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(f_vec).end(), nullptr);

        auto& upgrade = f_vec.upgrade();

        ASSERT_FALSE(upgrade.has_storage());
        ASSERT_DEATH((void)upgrade.front(), ".*");
        ASSERT_DEATH((void)upgrade.back(), ".*");
        ASSERT_DEATH((void)upgrade[0U], ".*");
        ASSERT_TRUE(upgrade.empty());
        ASSERT_EQ(upgrade.size(), 0U);
        ASSERT_EQ(upgrade.capacity(), 0U);
        ASSERT_EQ(upgrade.begin(), nullptr);
        ASSERT_EQ(upgrade.end(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(upgrade).begin(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(upgrade).end(), nullptr);
    });

    //Invariant: During all of the next operations, no allocations are performed

    vec_t             vec1{0U};
    test_predicate_t::operator()(vec1);

    vec_t             vec2 = std::move(vec1);
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);

    vec_t             vec3 = vec1;
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec3);

    vec3 = std::move(vec2);
    vec1 = std::move(vec3);
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);
    test_predicate_t::operator()(vec3);

    vec3 = vec2;
    vec1 = vec3;
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);
    test_predicate_t::operator()(vec3);
}

TEST(SkylakeVector, Basics_CapacityCtor) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>>;

    using test_predicate_t = decltype([](vec_t& f_vec) static {
        ASSERT_FALSE(f_vec.has_storage());
        ASSERT_DEATH((void)f_vec.front(), ".*");
        ASSERT_DEATH((void)f_vec.back(), ".*");
        ASSERT_DEATH((void)f_vec[0U], ".*");
        ASSERT_TRUE(f_vec.empty());
        ASSERT_EQ(f_vec.size(), 0U);
        ASSERT_EQ(f_vec.capacity(), 0U);
        ASSERT_EQ(f_vec.begin(), nullptr);
        ASSERT_EQ(f_vec.end(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(f_vec).begin(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(f_vec).end(), nullptr);

        auto& upgrade = f_vec.upgrade();

        ASSERT_FALSE(upgrade.has_storage());
        ASSERT_DEATH((void)upgrade.front(), ".*");
        ASSERT_DEATH((void)upgrade.back(), ".*");
        ASSERT_DEATH((void)upgrade[0U], ".*");
        ASSERT_TRUE(upgrade.empty());
        ASSERT_EQ(upgrade.size(), 0U);
        ASSERT_EQ(upgrade.capacity(), 0U);
        ASSERT_EQ(upgrade.begin(), nullptr);
        ASSERT_EQ(upgrade.end(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(upgrade).begin(), nullptr);
        ASSERT_EQ(static_cast<const vec_t&>(upgrade).end(), nullptr);
    });

    //Invariant: During all of the next operations, no allocations must be performed

    vec_t             vec1{0U};
    test_predicate_t::operator()(vec1);

    vec_t             vec2 = std::move(vec1);
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);

    vec_t             vec3 = vec1;
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec3);

    vec3 = std::move(vec2);
    vec1 = std::move(vec3);
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);
    test_predicate_t::operator()(vec3);

    vec3 = vec2;
    vec1 = vec3;
    test_predicate_t::operator()(vec1);
    test_predicate_t::operator()(vec2);
    test_predicate_t::operator()(vec3);
}
#endif

TEST(SkylakeVector, Basics_PushBackOnNoStorage) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>>;

    vec_t vec1{0U};

    vec1.upgrade().push_back(std::make_shared<trivial_t>());

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_FALSE(vec1.empty());
    ASSERT_EQ(vec1.size(), 1U);
    ASSERT_EQ(vec1.capacity(), vec_t::CInitialCapacity);

    auto& upgrade = vec1.upgrade();

    ASSERT_TRUE(upgrade.has_storage());
    ASSERT_FALSE(upgrade.empty());
    ASSERT_EQ(upgrade.size(), 1U);
    ASSERT_EQ(upgrade.capacity(), vec_t::CInitialCapacity);
}

TEST(SkylakeVector, Basics_EmplaceBackOnNoStorage) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>>;

    vec_t vec1{0U};

    auto  object = std::make_shared<trivial_t>();
    auto* ptr    = object.get();
    vec1.upgrade().emplace_back(std::move(object));

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_FALSE(vec1.empty());
    ASSERT_EQ(vec1.size(), 1U);
    ASSERT_EQ(vec1.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, vec1[0U].get());

    auto& upgrade = vec1.upgrade();

    ASSERT_TRUE(upgrade.has_storage());
    ASSERT_FALSE(upgrade.empty());
    ASSERT_EQ(upgrade.size(), 1U);
    ASSERT_EQ(upgrade.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, upgrade[0U].get());
}

TEST(SkylakeVector, Basics_PushBack) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>>;

    vec_t vec1{0U};

    auto  object = std::make_shared<trivial_t>();
    auto* ptr    = object.get();
    vec1.upgrade().push_back(object);

    ASSERT_EQ(object.use_count(), 2U);

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_FALSE(vec1.empty());
    ASSERT_EQ(vec1.size(), 1U);
    ASSERT_EQ(vec1.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, vec1[0U].get());

    auto& upgrade = vec1.upgrade();

    ASSERT_TRUE(upgrade.has_storage());
    ASSERT_FALSE(upgrade.empty());
    ASSERT_EQ(upgrade.size(), 1U);
    ASSERT_EQ(upgrade.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, upgrade[0U].get());

    vec1.clear();
    ASSERT_EQ(object.use_count(), 1U);
}

TEST(SkylakeVector, Basics_EmplaceBack) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>>;

    vec_t vec1{0U};

    auto  object = std::make_shared<trivial_t>();
    auto* ptr    = object.get();
    vec1.upgrade().emplace_back(std::move(object));

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_FALSE(vec1.empty());
    ASSERT_EQ(vec1.size(), 1U);
    ASSERT_EQ(vec1.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, vec1[0U].get());

    ASSERT_EQ(vec1[0U].use_count(), 1U);

    auto& upgrade = vec1.upgrade();

    ASSERT_TRUE(upgrade.has_storage());
    ASSERT_FALSE(upgrade.empty());
    ASSERT_EQ(upgrade.size(), 1U);
    ASSERT_EQ(upgrade.capacity(), vec_t::CInitialCapacity);
    ASSERT_EQ(ptr, upgrade[0U].get());
}

#if !SKL_BUILD_SHIPPING
TEST(SkylakeVector, Basics_AllocateOnDefaultCtor_DefaultCtor) {
    struct trivial_t {
        byte body[1024];
    };

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>, skl::CVectorIncreaseStep>;

    using test_predicate_t = decltype([](vec_t& f_vec) static {
        ASSERT_TRUE(f_vec.has_storage());
        ASSERT_DEATH((void)f_vec.front(), ".*");
        ASSERT_DEATH((void)f_vec.back(), ".*");
        ASSERT_DEATH((void)f_vec[0U], ".*");
        ASSERT_TRUE(f_vec.empty());
        ASSERT_EQ(f_vec.size(), 0U);
        ASSERT_EQ(f_vec.capacity(), vec_t::CInitialCapacity);
        ASSERT_NE(f_vec.begin(), nullptr);
        ASSERT_NE(f_vec.end(), nullptr);
        ASSERT_NE(static_cast<const vec_t&>(f_vec).begin(), nullptr);
        ASSERT_NE(static_cast<const vec_t&>(f_vec).end(), nullptr);

        auto& upgrade = f_vec.upgrade();

        ASSERT_TRUE(upgrade.has_storage());
        ASSERT_DEATH((void)upgrade.front(), ".*");
        ASSERT_DEATH((void)upgrade.back(), ".*");
        ASSERT_DEATH((void)upgrade[0U], ".*");
        ASSERT_TRUE(upgrade.empty());
        ASSERT_EQ(upgrade.size(), 0U);
        ASSERT_EQ(upgrade.capacity(), vec_t::CInitialCapacity);
        ASSERT_NE(upgrade.begin(), nullptr);
        ASSERT_NE(upgrade.end(), nullptr);
        ASSERT_NE(static_cast<const vec_t::atrp_type&>(upgrade).begin(), nullptr);
        ASSERT_NE(static_cast<const vec_t::atrp_type&>(upgrade).end(), nullptr);
    });

    vec_t             vec1;
    test_predicate_t::operator()(vec1);

    vec_t vec2 = std::move(vec1);
    ASSERT_FALSE(vec1.has_storage());
    test_predicate_t::operator()(vec2);

    vec_t vec3 = vec1;
    ASSERT_FALSE(vec1.has_storage());
    ASSERT_FALSE(vec3.has_storage());

    vec1 = vec2;
    ASSERT_FALSE(vec1.has_storage());
    test_predicate_t::operator()(vec2);

    vec3 = vec1;
    ASSERT_FALSE(vec1.has_storage());

    //In order to trigger an allocation on copy, we need at least one item in vec2 (which has the storage)
    auto object = std::make_shared<trivial_t>();

    vec2.upgrade().push_back(object);

    ASSERT_EQ(2U, object.use_count());

    vec1 = vec2;
    vec3 = vec1;

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_TRUE(vec2.has_storage());
    ASSERT_TRUE(vec3.has_storage());

    ASSERT_EQ(4U, object.use_count());

    ASSERT_NE(vec1.data(), vec2.data());
    ASSERT_NE(vec3.data(), vec2.data());

    vec1.clear();
    ASSERT_EQ(3U, object.use_count());

    vec2.clear();
    ASSERT_EQ(2U, object.use_count());

    vec3.clear();
    ASSERT_EQ(1U, object.use_count());

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_TRUE(vec2.has_storage());
    ASSERT_TRUE(vec3.has_storage());
}
#endif

TEST(SkylakeVector, Basics_PushToCapacity_NoNewAllocation) {
    struct trivial_t {
        byte body[1024];
    };

    constexpr u64 CCapacity = 128U;

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>, CCapacity>;

    vec_t vec1{};

    auto* data = vec1.data();

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), 0U);

    for (u64 i = 0U; i < CCapacity; ++i) {
        vec1.upgrade().push_back({});
    }

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), CCapacity);

    for (u64 i = 0U; i < CCapacity; ++i) {
        ASSERT_EQ(nullptr, vec1[i]);
    }

    vec1.clear();

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), 0U);

    for (u64 i = 0U; i < CCapacity; ++i) {
        vec1.upgrade().push_back(std::make_shared<trivial_t>());
    }

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), CCapacity);

    for (u64 i = 0U; i < CCapacity; ++i) {
        ASSERT_NE(nullptr, vec1[i]);
    }

    ASSERT_EQ(data, vec1.data());
}

TEST(SkylakeVector, Basics_PushToCapacity_OneNewAllocation) {
    struct trivial_t {
        byte body[1024];
    };

    constexpr u64 CCapacity = 128U;

    using vec_t = skl::skl_vector<std::shared_ptr<trivial_t>, CCapacity / 2U>;

    vec_t vec1{};

    auto* data = vec1.data();

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity / 2U);
    ASSERT_EQ(vec1.size(), 0U);

    for (u64 i = 0U; i < CCapacity; ++i) {
        vec1.upgrade().push_back({});
    }

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), CCapacity);

    for (u64 i = 0U; i < CCapacity; ++i) {
        ASSERT_EQ(nullptr, vec1[i]);
    }

    vec1.clear();

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), 0U);

    for (u64 i = 0U; i < CCapacity; ++i) {
        vec1.upgrade().push_back(std::make_shared<trivial_t>());
    }

    ASSERT_TRUE(vec1.has_storage());
    ASSERT_EQ(vec1.capacity(), CCapacity);
    ASSERT_EQ(vec1.size(), CCapacity);

    for (u64 i = 0U; i < CCapacity; ++i) {
        ASSERT_NE(nullptr, vec1[i]);
    }

    ASSERT_NE(data, vec1.data());
}
