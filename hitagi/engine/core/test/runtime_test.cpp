#include "test_macros.hpp"
import std;
import core;
import interop.gtest;
import interop.spdlog;

TEST(RuntimeTest, RestoresAllocatorAndLoggingAfterJobsAndIOFinish) {
    auto*      previous = std::pmr::get_default_resource();
    const auto level    = spdlog::get_level();
    const auto path     = std::filesystem::temp_directory_path() / "hitagi-runtime-io.txt";
    {
        hitagi::core::Runtime runtime({.worker_count = 2, .log_level = "off"});
        EXPECT_NE(std::pmr::get_default_resource(), previous);
        runtime.IO().SaveString(path, "before");
        const auto                     retained = runtime.IO().SyncOpenAndReadBinary(path);
        std::vector<std::future<void>> readers;
        for (int i = 0; i < 8; ++i) readers.push_back(runtime.Jobs().Submit([&] {
            for (int n = 0; n < 20; ++n) {
                const auto data = runtime.IO().SyncOpenAndReadBinary(path);
                EXPECT_TRUE(data.Str() == "before" || data.Str() == "after");
            }
        }));
        runtime.IO().SaveString(path, "after");
        for (auto& reader : readers) reader.get();
        EXPECT_EQ(retained.Str(), "before");
        EXPECT_EQ(runtime.IO().SyncOpenAndReadBinary(path).Str(), "after");
    }
    EXPECT_EQ(std::pmr::get_default_resource(), previous);
    EXPECT_EQ(spdlog::get_level(), level);
    std::filesystem::remove(path);
}
