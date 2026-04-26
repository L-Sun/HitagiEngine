#include "test_macros.hpp"
#include <spdlog/spdlog.h>

import core;
import test_utils;

using namespace hitagi::core;

TEST(MemoryTest, BufferSpan) {
    Buffer buf(32);
    auto   sp = buf.Span<int>();
    for (auto& item : sp) {
        item = 1;
    }
    auto p = reinterpret_cast<int*>(buf.GetData());
    for (size_t i = 0; i < 32 / sizeof(int); i++) {
        EXPECT_EQ(sp[i], p[i]);
    }
}

TEST(MemoryTest, Allocate) {
    MemoryPool pool{spdlog::default_logger()};
    EXPECT_NO_THROW({
        auto p = pool.allocate(16);
        pool.deallocate(p, 16);
    });
}

TEST(MemoryTest, PmrContainer) {
    MemoryPool            pool{spdlog::default_logger()};
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
