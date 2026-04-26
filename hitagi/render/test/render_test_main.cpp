#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

import std;
import core;

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::debug);

    auto memory_manager = std::make_unique<hitagi::core::MemoryManager>();
    auto file_manager   = std::make_unique<hitagi::core::FileIOManager>();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
