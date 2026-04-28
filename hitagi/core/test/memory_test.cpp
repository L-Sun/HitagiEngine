#include "test_macros.hpp"

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
    auto* memory_manager = static_cast<MemoryManager*>(RuntimeModule::GetModule("MemoryManager"));
    ASSERT_NE(memory_manager, nullptr);

    auto allocator = memory_manager->GetAllocator<>();
    EXPECT_NO_THROW({
        auto* p = allocator.allocate_bytes(16);
        allocator.deallocate_bytes(p, 16);
    });
}

TEST(MemoryTest, PmrContainer) {
    auto* memory_manager = static_cast<MemoryManager*>(RuntimeModule::GetModule("MemoryManager"));
    ASSERT_NE(memory_manager, nullptr);

    std::pmr::vector<int> vec{memory_manager->GetAllocator<int>()};
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
