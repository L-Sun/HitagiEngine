#include <hitagi/utils/test.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <vector>
#include "hitagi/core/buffer.hpp"

using namespace hitagi;

TEST(MemoryTest, BufferSpan) {
    core::Buffer buf(32);
    auto         sp = buf.Span<int>();
    for (auto& item : sp) {
        item = 1;
    }
    auto p = reinterpret_cast<int*>(buf.GetData());
    for (size_t i = 0; i < 32 / sizeof(int); i++) {
        EXPECT_EQ(sp[i], p[i]);
    }
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
    spdlog::set_level(spdlog::level::off);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
