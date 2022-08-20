#include <hitagi/utils/test.hpp>
#include <hitagi/utils/soa.hpp>

#include <array>
#include "gtest/gtest.h"

using namespace hitagi::utils;

class debug_memory_resource : public std::pmr::memory_resource {
public:
    debug_memory_resource(std::string_view name, std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : name(name), upstream(upstream) {}
    debug_memory_resource(const debug_memory_resource&)            = delete;
    debug_memory_resource& operator=(const debug_memory_resource&) = delete;

private:
    [[nodiscard]] void* do_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) final {
        fmt::print("allocate {} byte(s)\n", bytes);
        return upstream->allocate(bytes, alignment);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) final {
        fmt::print("deallocate pointer: {}, {} byte(s)\n", p, bytes);
        upstream->deallocate(p, bytes, alignment);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final {
        return this == &other;
    }

    std::pmr::string           name;
    std::pmr::memory_resource* upstream;
};

TEST(SoaTest, Init) {
    debug_memory_resource res{"Init"};

    SoA<
        std::array<float, 3>,  // POSITION
        std::array<float, 2>   // UV
        >
        data;
    SoA<
        std::array<float, 3>,  // POSITION
        std::array<float, 2>   // UV
        >
        data_with_pmr{&res};
}

TEST(SoaTest, AccessElement) {
    debug_memory_resource res{"AccessElement"};

    SoA<int, float> data{&res};
    data.push_back({3, 3.0f});
    data.push_back({4, 4.0f});

    // No const
    {
        auto& no_const_data = data;

        {
            auto x = no_const_data[0];
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = no_const_data.at(0);
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = no_const_data.front();
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = no_const_data.back();
            EXPECT_EQ(std::get<0>(x), 4);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 4);
        }
        {
            EXPECT_EQ(no_const_data.element_at<0>(0), 3);
            EXPECT_EQ(no_const_data.element_at<0>(1), 4);
            EXPECT_DOUBLE_EQ(no_const_data.element_at<1>(0), 3);
            EXPECT_DOUBLE_EQ(no_const_data.element_at<1>(1), 4);
        }
    }
    // Const
    {
        const auto& const_data = data;

        {
            auto x = const_data[0];
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = const_data.at(0);
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = const_data.front();
            EXPECT_EQ(std::get<0>(x), 3);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 3);
        }
        {
            auto x = const_data.back();
            EXPECT_EQ(std::get<0>(x), 4);
            EXPECT_DOUBLE_EQ(std::get<1>(x), 4);
        }
        {
            EXPECT_EQ(const_data.element_at<0>(0), 3);
            EXPECT_EQ(const_data.element_at<0>(1), 4);
            EXPECT_DOUBLE_EQ(const_data.element_at<1>(0), 3);
            EXPECT_DOUBLE_EQ(const_data.element_at<1>(1), 4);
        }
    }
}

TEST(SoaTest, ModifyElement) {
    debug_memory_resource res{"Init"};

    SoA<int, float> data{&res};
    data.push_back({3, 3.0f});
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data.element_at<0>(0), 3);
    EXPECT_FLOAT_EQ(data.element_at<1>(0), 3.0f);

    auto [_int, _float] = data.emplace_back(4, 4.0f);
    EXPECT_EQ(data.size(), 2);
    EXPECT_EQ(_int, 4);
    EXPECT_FLOAT_EQ(_float, 4.0f);

    data.pop_back();
    EXPECT_EQ(data.size(), 1);
    data.resize(2);
    EXPECT_EQ(data.size(), 2);
    data.resize(3, {5, 5.0f});
    EXPECT_EQ(data.size(), 3);
    data.clear();
    EXPECT_EQ(data.size(), 0);

    SoA<int, float> data2{&res};
    data2.push_back({1, 1.0f});
    data.emplace_back(2, 2.0f);
}

TEST(SoaTest, Iteration) {
    debug_memory_resource res{"Init"};
    SoA<int, float>       data{&res};

    data.emplace_back(1, 1.0f);
    data.emplace_back(2, 2.0f);
    data.emplace_back(3, 3.0f);

    int i = 1;
    for (auto [_i, _f] : const_cast<const decltype(data)&>(data)) {
        EXPECT_EQ(_i, i);
        EXPECT_FLOAT_EQ(_f, static_cast<float>(i));
        i++;
    }

    i = 1;
    for (auto [_i, _f] : data) {
        EXPECT_EQ(_i, i);
        EXPECT_FLOAT_EQ(_f, static_cast<float>(i));
        i++;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}