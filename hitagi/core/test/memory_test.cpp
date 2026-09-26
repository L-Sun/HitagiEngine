#include "test_macros.hpp"

import std;
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

// The test binary's main() installs a MemoryManager, which replaces the PMR
// default resource with the engine pool. These tests exercise that pool
// through the default resource rather than looking the manager up.
TEST(MemoryTest, InstallsPoolAsDefaultResource) {
    EXPECT_NE(std::pmr::get_default_resource(), std::pmr::new_delete_resource());
}

TEST(MemoryTest, Allocate) {
    std::pmr::polymorphic_allocator<> allocator{std::pmr::get_default_resource()};
    EXPECT_NO_THROW({
        auto* p = allocator.allocate_bytes(16);
        allocator.deallocate_bytes(p, 16);
    });
}

TEST(MemoryTest, PmrContainer) {
    std::pmr::vector<int> vec{std::pmr::polymorphic_allocator<int>{std::pmr::get_default_resource()}};
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
