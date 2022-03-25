#include <hitagi/core/memory_manager.hpp>
#include <hitagi/utils/test.hpp>

#include <fmt/format.h>

#include <vector>
#include <algorithm>
#include <numeric>

using namespace hitagi;

template <typename Iter>
void print_line(int offset, Iter begin, const Iter end) {
    fmt::print("(dec) {:02x}: {:3}\n", offset, fmt::join(begin, end, "   "));
    fmt::print("(hex) {:02x}:  {:02x}\n", offset, fmt::join(begin, end, "    "));

    fmt::print("(asc) {:02x}:", offset);
    std::for_each(begin, end, [](const auto& c) {
        if (std::isgraph(static_cast<int>(c))) {
            fmt::print(" {:>3}  ", static_cast<char>(c));
        } else {
            fmt::print("\\{:03o}  ", c);
        }
    });
    fmt::print("\n");
}

template <typename Buffer, typename Container>
void print_buffer(const std::string_view title, const Buffer& buffer, const Container& container) {
    fmt::print("=========={:^10}==========\n", title);

    auto begin = buffer.begin();
    fmt::print("Buffer Address Start: {}\n", static_cast<const void*>(buffer.data()));
    for (const auto& element : container) {
        fmt::print(" Item Address: {}\n", static_cast<const void*>(&element));
    }

    std::array<int, 16> head;
    std::iota(head.begin(), head.end(), 0);
    fmt::print("          {:>3x}\n", fmt::join(head, "   "));

    for (size_t offset = 0; offset < buffer.size(); offset += 16) {
        print_line(offset, std::next(begin, offset), std::next(begin, offset + 16));
    }

    fmt::print("\n");
}

TEST(MemoryTest, Allocate) {
    core::MemoryPool pool{};
    EXPECT_NO_THROW({
        auto p = pool.allocate(16);
        pool.deallocate(p, 16);
    });
}

TEST(MemoryTest, PmrContainer) {
    core::MemoryPool      pool{};
    std::pmr::vector<int> vec{&pool};
    EXPECT_NO_THROW({
        for (size_t i = 0; i < 10000; i++) {
            vec.push_back(i);
        }
    });
    auto p = vec.data();
    for (size_t i = 0; i < 10000; i++) {
        EXPECT_EQ(*p++, i);
    }
}

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
