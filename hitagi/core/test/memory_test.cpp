#include <hitagi/utils/test.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/buffer.hpp>

#include <vector>

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
    ::testing::InitGoogleTest(&argc, argv);
    g_MemoryManager->Initialize();

    spdlog::set_level(spdlog::level::off);
    int result = RUN_ALL_TESTS();

    g_MemoryManager->Finalize();
    return result;
}
