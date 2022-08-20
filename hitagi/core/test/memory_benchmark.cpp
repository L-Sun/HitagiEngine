#include <hitagi/utils/test.hpp>
#include <hitagi/core/memory_manager.hpp>

#include <vector>

using namespace hitagi;

static void BM_DefaultAllocate(benchmark::State& state) {
    for (auto _ : state) {
        std::pmr::vector<std::pmr::string> strs;
        std::pmr::string                   str1 = "hello world";
        std::pmr::string                   str2 = "a fox jumps over the lazy dog";

        for (size_t i = 0; i < 100; i++) {
            strs.emplace_back((i & 0x1) ? str1 : str2);
        }
    }
}
BENCHMARK(BM_DefaultAllocate);

static void BM_StdUnsynchronized(benchmark::State& state) {
    std::pmr::unsynchronized_pool_resource res{};

    for (auto _ : state) {
        std::pmr::vector<std::pmr::string> strs{&res};
        std::pmr::string                   str1 = "hello world";
        std::pmr::string                   str2 = "a fox jumps over the lazy dog";

        for (size_t i = 0; i < 100; i++) {
            strs.emplace_back((i & 0x1) ? str1 : str2);
        }
    }
}
BENCHMARK(BM_StdUnsynchronized);

static void BM_StdSynchronized(benchmark::State& state) {
    std::pmr::synchronized_pool_resource res{};

    for (auto _ : state) {
        std::pmr::vector<std::pmr::string> strs{&res};
        std::pmr::string                   str1 = "hello world";
        std::pmr::string                   str2 = "a fox jumps over the lazy dog";

        for (size_t i = 0; i < 100; i++) {
            strs.emplace_back((i & 0x1) ? str1 : str2);
        }
    }
}
BENCHMARK(BM_StdSynchronized);

static void BM_PmrAllocate(benchmark::State& state) {
    core::MemoryPool pool{};

    for (auto _ : state) {
        std::pmr::vector<std::pmr::string> strs{&pool};
        std::pmr::string                   str1 = "hello world";
        std::pmr::string                   str2 = "a fox jumps over the lazy dog";

        for (size_t i = 0; i < 100; i++) {
            strs.emplace_back((i & 0x1) ? str1 : str2);
        }
    }
}
BENCHMARK(BM_PmrAllocate);

BENCHMARK_MAIN();
